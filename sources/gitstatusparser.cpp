// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "gitstatusparser.h"

namespace ChangesPanel {

static FileState stateForStatusChar(char16_t statusChar)
{
    switch (statusChar) {
    case u'M':
    case u'T': return FileState::Modified;
    case u'?': return FileState::Untracked;
    case u'A': return FileState::Added;
    case u'R':
    case u'C': return FileState::Renamed;
    case u'D': return FileState::Deleted;
    case u'U': return FileState::Unmerged;
    default:  return FileState::Unknown;
    }
}

static bool isUnmergedCode(char16_t indexChar, char16_t workTreeChar)
{
    return indexChar == u'U' || workTreeChar == u'U'
           || (indexChar == u'A' && workTreeChar == u'A')
           || (indexChar == u'D' && workTreeChar == u'D');
}

static FileState dominantState(FileState indexState, FileState workTreeState)
{
    static constexpr FileState precedence[] = {
        FileState::Unmerged,
        FileState::Renamed,
        FileState::Deleted,
        FileState::Modified,
        FileState::Added,
        FileState::Untracked,
    };
    for (FileState state : precedence) {
        if (indexState == state || workTreeState == state)
            return state;
    }
    return FileState::Unknown;
}

QString unquoteGitPath(const QString &path)
{
    if (path.size() < 2 || !path.startsWith(u'"') || !path.endsWith(u'"'))
        return path;
    const QByteArray quoted = path.sliced(1, path.size() - 2).toUtf8();
    QByteArray bytes;
    bytes.reserve(quoted.size());
    for (qsizetype i = 0; i < quoted.size(); ++i) {
        const char c = quoted.at(i);
        if (c != '\\') {
            bytes.append(c);
            continue;
        }
        if (++i >= quoted.size())
            break;
        const char escaped = quoted.at(i);
        switch (escaped) {
        case 'a': bytes.append('\a'); break;
        case 'b': bytes.append('\b'); break;
        case 'f': bytes.append('\f'); break;
        case 'n': bytes.append('\n'); break;
        case 'r': bytes.append('\r'); break;
        case 't': bytes.append('\t'); break;
        case 'v': bytes.append('\v'); break;
        default:
            if (escaped >= '0' && escaped <= '7') {
                int value = 0;
                qsizetype digits = 0;
                while (digits < 3 && i < quoted.size()) {
                    const char digit = quoted.at(i);
                    if (digit < '0' || digit > '7')
                        break;
                    value = value * 8 + (digit - '0');
                    ++i;
                    ++digits;
                }
                --i;
                bytes.append(char(value));
            } else {
                bytes.append(escaped);
            }
        }
    }
    return QString::fromUtf8(bytes);
}

QStringList splitRenameLine(const QString &pathPart)
{
    static const QString arrow = QStringLiteral(" -> ");
    QString oldPath;
    QString newPath;
    if (pathPart.startsWith(u'"')) {
        qsizetype close = -1;
        for (qsizetype i = 1; i < pathPart.size(); ++i) {
            if (pathPart.at(i) == u'\\') {
                ++i;
                continue;
            }
            if (pathPart.at(i) == u'"') {
                close = i;
                break;
            }
        }
        if (close < 0 || !QStringView(pathPart).mid(close + 1).startsWith(arrow))
            return {};
        oldPath = unquoteGitPath(pathPart.left(close + 1));
        newPath = pathPart.mid(close + 1 + arrow.size());
    } else {
        const qsizetype pos = pathPart.indexOf(arrow);
        if (pos < 0)
            return {};
        oldPath = pathPart.left(pos);
        newPath = pathPart.mid(pos + arrow.size());
    }
    newPath = unquoteGitPath(newPath);
    if (oldPath.isEmpty() || newPath.isEmpty())
        return {};
    return {oldPath, newPath};
}

static bool isRecordedInIndex(FileState indexState)
{
    return indexState != FileState::Unknown && indexState != FileState::Untracked;
}

GitStatus parseStatusOutput(const QString &output)
{
    GitStatus status;
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (QString line : lines) {
        if (line.endsWith('\r'))
            line.chop(1);
        if (line.size() <= 3 || line.at(2) != u' ')
            continue;

        const char16_t indexChar = line.at(0).unicode();
        const char16_t workTreeChar = line.at(1).unicode();
        const FileState indexState = stateForStatusChar(indexChar);
        const FileState workTreeState = stateForStatusChar(workTreeChar);
        const FileState state = isUnmergedCode(indexChar, workTreeChar)
                                    ? FileState::Unmerged
                                    : dominantState(indexState, workTreeState);
        if (state == FileState::Unknown)
            continue;

        QString relativePath;
        const bool hasRenameArrow = indexChar == u'R' || indexChar == u'C'
                                    || workTreeChar == u'R' || workTreeChar == u'C';
        if (hasRenameArrow) {
            const QStringList files = splitRenameLine(line.mid(3));
            if (files.size() != 2)
                continue;
            relativePath = files.at(1);
        } else {
            relativePath = unquoteGitPath(line.mid(3));
        }
        if (relativePath.isEmpty())
            continue;

        status.fileStates.insert(relativePath, state);

        if (state != FileState::Unmerged && isRecordedInIndex(indexState))
            status.stagedFiles.insert(relativePath);
    }
    return status;
}

BranchInfo parseBranchHeader(const QString &output)
{
    BranchInfo info;
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (QString line : lines) {
        if (line.endsWith('\r'))
            line.chop(1);
        if (!line.startsWith("## "))
            continue;

        QString rest = line.mid(3);
        if (rest.startsWith("HEAD (no branch)")) {
            info.branch = "HEAD";
            return info;
        }
        static const QString kNoCommits = "No commits yet on ";
        if (rest.startsWith(kNoCommits)) {
            info.branch = rest.mid(kNoCommits.size()).trimmed();
            return info;
        }

        QString bracketPart;
        const qsizetype bracket = rest.indexOf(" [");
        if (bracket >= 0 && rest.endsWith(']')) {
            bracketPart = rest.mid(bracket + 2, rest.size() - (bracket + 2) - 1);
            rest = rest.left(bracket);
        }

        const qsizetype sep = rest.indexOf("...");
        if (sep >= 0) {
            info.branch = rest.left(sep);
            info.hasUpstream = true;
        } else {
            info.branch = rest;
        }

        if (!bracketPart.isEmpty()) {
            const QStringList parts = bracketPart.split(',', Qt::SkipEmptyParts);
            for (const QString &part : parts) {
                const QString token = part.trimmed();
                if (token.startsWith("ahead "))
                    info.ahead = token.mid(6).toInt();
                else if (token.startsWith("behind "))
                    info.behind = token.mid(7).toInt();
            }
        } else if (info.hasUpstream) {
            info.ahead = 0;
            info.behind = 0;
        }
        return info;
    }
    return info;
}

QStringList parseSubmoduleStatusLines(const QStringList &lines)
{
    QStringList submodules;
    for (const QString &line : lines) {
        if (line.isEmpty() || line.startsWith('-'))
            continue;
        const QString rest = line.mid(1);
        const qsizetype shaEnd = rest.indexOf(u' ');
        if (shaEnd < 0)
            continue;
        QString path = rest.mid(shaEnd + 1);
        if (path.endsWith(u')')) {
            const qsizetype describe = path.lastIndexOf(QStringLiteral(" ("));
            if (describe > 0)
                path = path.left(describe);
        }
        if (!path.isEmpty())
            submodules.append(path);
    }
    return submodules;
}

} // namespace ChangesPanel

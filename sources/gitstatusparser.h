// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

namespace ChangesPanel {

enum class FileState : quint8 {
    Unknown = 0,
    Untracked,
    Added,
    Modified,
    Deleted,
    Renamed,
    Unmerged,
};

using FileStateMap = QHash<QString, FileState>;

struct GitStatus
{
    FileStateMap fileStates;
    QSet<QString> stagedFiles;

    bool operator==(const GitStatus &other) const = default;
};

struct BranchInfo
{
    QString branch;
    int ahead = -1;
    int behind = -1;
    bool hasUpstream = false;

    bool operator==(const BranchInfo &other) const = default;
};

GitStatus parseStatusOutput(const QString &output);
BranchInfo parseBranchHeader(const QString &output);
QStringList parseSubmoduleStatusLines(const QStringList &lines);
QString unquoteGitPath(const QString &path);
QStringList splitRenameLine(const QString &pathPart);

} // namespace ChangesPanel

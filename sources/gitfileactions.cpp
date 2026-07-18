// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "gitfileactions.h"

#include "changespanelsettings.h"
#include "changespaneltr.h"
#include "gitcommands.h"
#include "gitstatustracker.h"

#include <coreplugin/icore.h>

#include <utils/commandline.h>
#include <utils/hostosinfo.h>
#include <utils/qtcprocess.h>

#include <QHash>
#include <QMessageBox>

using namespace Utils;

namespace ChangesPanel {

StageAction stageActionFor(FileState state, bool staged)
{
    if (staged)
        return StageAction::Unstage;
    switch (state) {
    case FileState::Added:
    case FileState::Modified:
    case FileState::Untracked:
    case FileState::Deleted:
        return StageAction::Stage;
    case FileState::Unknown:
    case FileState::Renamed:
    case FileState::Unmerged:
        return StageAction::None;
    }
    return StageAction::None;
}

QString stageActionToolTip(StageAction action, const QString &fileName)
{
    switch (action) {
    case StageAction::Stage:   return Tr::tr("Stage \"%1\"").arg(fileName);
    case StageAction::Unstage: return Tr::tr("Unstage \"%1\"").arg(fileName);
    case StageAction::None:    break;
    }
    return {};
}

QString stageActionFailureTitle(StageAction action)
{
    return action == StageAction::Unstage ? Tr::tr("Unstage Failed") : Tr::tr("Stage Failed");
}

bool isRevertable(FileState state)
{
    return state == FileState::Modified || state == FileState::Deleted;
}

static FileEntryAction guardOpenForDeleted(FileEntryAction action, FileState state)
{
    if (action == FileEntryAction::OpenEditor && state == FileState::Deleted)
        return FileEntryAction::None;
    return action;
}

FileEntryAction diffColumnActionFor(FileState state)
{
    const bool clickDiffs = settings().fileClickAction() == ChangesPanelSettings::OpenDiff;
    return guardOpenForDeleted(
        clickDiffs ? FileEntryAction::OpenEditor : FileEntryAction::ShowDiff, state);
}

FileEntryAction rowClickActionFor(FileState state)
{
    const bool clickDiffs = settings().fileClickAction() == ChangesPanelSettings::OpenDiff;
    return guardOpenForDeleted(
        clickDiffs ? FileEntryAction::ShowDiff : FileEntryAction::OpenEditor, state);
}

QString fileEntryActionToolTip(FileEntryAction action, const QString &fileName)
{
    switch (action) {
    case FileEntryAction::OpenEditor: return Tr::tr("Open \"%1\"").arg(fileName);
    case FileEntryAction::ShowDiff:   return Tr::tr("Diff \"%1\"").arg(fileName);
    case FileEntryAction::None:       break;
    }
    return {};
}

static QString stageActionFailureText(StageAction action, const QList<StageableFile> &files)
{
    if (files.size() == 1) {
        const QString fileName
            = files.first().repository.pathAppended(files.first().relativePath).fileName();
        return action == StageAction::Unstage
                   ? Tr::tr("Could not unstage \"%1\".").arg(fileName)
                   : Tr::tr("Could not stage \"%1\".").arg(fileName);
    }
    return action == StageAction::Unstage ? Tr::tr("Could not unstage all files.")
                                          : Tr::tr("Could not stage all files.");
}

GitFileActions::GitFileActions(GitCommands &git, GitStatusTracker &tracker, QObject *parent)
    : QObject(parent)
    , m_git(git)
    , m_tracker(tracker)
{}

Result<> GitFileActions::applyStageAction(StageAction action, const QList<StageableFile> &files)
{
    if (action == StageAction::None || files.isEmpty())
        return ResultOk;

    struct Batch
    {
        QStringList paths;
        QStringList intentToAdd;
    };
    QHash<FilePath, Batch> batches;
    for (const StageableFile &file : files) {
        Batch &batch = batches[file.repository];
        batch.paths.append(file.relativePath);
        if (action == StageAction::Unstage && file.state == FileState::Added)
            batch.intentToAdd.append(file.relativePath);
    }

    QStringList errors;
    for (auto it = batches.cbegin(); it != batches.cend(); ++it) {
        const Batch &batch = it.value();
        const Result<> result = action == StageAction::Unstage
                                    ? m_git.unstageFiles(it.key(), batch.paths,
                                                         batch.intentToAdd)
                                    : m_git.stageFiles(it.key(), batch.paths);
        if (!result)
            errors.append(result.error());
        m_tracker.requestRefresh(it.key());
    }
    if (errors.isEmpty())
        return ResultOk;
    return ResultError(stageActionFailureText(action, files) + '\n' + errors.join('\n'));
}

Result<> GitFileActions::revertFile(const FilePath &repository, const QString &relativePath)
{
    const Result<> result = m_git.checkoutFiles(repository, {relativePath});
    m_tracker.requestRefresh(repository);
    if (result)
        return result;
    return ResultError(Tr::tr("Could not revert \"%1\": %2")
                           .arg(repository.pathAppended(relativePath).fileName(),
                                result.error()));
}

void GitFileActions::diffFile(const FilePath &repository, const QString &relativePath, bool staged)
{
    m_git.showDiff(repository, relativePath, staged);
}

void GitFileActions::openInExternalGitClient(const FilePath &repository)
{
    QStringList parts = ProcessArgs::splitArgs(
        settings().externalGitClient().trimmed(), HostOsInfo::hostOs());
    if (parts.isEmpty()) {
        QMessageBox::warning(
            Core::ICore::dialogParent(),
            Tr::tr("Open in Git Client"),
            Tr::tr("No external Git client is configured. Set the command in "
                   "Preferences > Version Control > Changes Panel."));
        return;
    }
    CommandLine command(FilePath::fromUserInput(parts.takeFirst()));
    command.addArgs(parts);
    command.addArg(repository.path());
    if (!Process::startDetached(command, repository)) {
        QMessageBox::warning(
            Core::ICore::dialogParent(),
            Tr::tr("Open in Git Client"),
            Tr::tr("Could not run \"%1\".").arg(command.toUserOutput()));
    }
}

} // namespace ChangesPanel

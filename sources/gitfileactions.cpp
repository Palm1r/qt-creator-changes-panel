// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "gitfileactions.h"

#include "changespanelsettings.h"
#include "changespaneltr.h"
#include "gitcommands.h"
#include "gitstatustracker.h"
#include "launchcommand.h"

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
    return revertFiles(repository, {relativePath});
}

Result<> GitFileActions::revertFiles(const FilePath &repository, const QStringList &relativePaths)
{
    if (relativePaths.isEmpty())
        return ResultOk;
    const Result<> result = m_git.checkoutFiles(repository, relativePaths);
    m_tracker.requestRefresh(repository);
    if (result)
        return result;
    if (relativePaths.size() == 1) {
        return ResultError(Tr::tr("Could not revert \"%1\": %2")
                               .arg(repository.pathAppended(relativePaths.first()).fileName(),
                                    result.error()));
    }
    return ResultError(Tr::tr("Could not revert all files: %1").arg(result.error()));
}

void GitFileActions::diffFile(const FilePath &repository, const QString &relativePath, bool staged)
{
    m_git.showDiff(repository, relativePath, staged);
}

void GitFileActions::diffAllChanges(const FilePath &repository,
                                    const QStringList &unstagedPaths,
                                    const QStringList &stagedPaths)
{
    m_git.showDiffAll(repository, unstagedPaths, stagedPaths);
}

static void runGitClient(
    const QString &configuredCommand,
    const FilePath &repository,
    const std::optional<QString> &relativeFilePath = std::nullopt)
{
    std::optional<QString> filePath = std::nullopt;
    if (relativeFilePath)
        filePath = repository.pathAppended(*relativeFilePath).path();
    QStringList arguments = expandedLaunchCommand(
        configuredCommand, HostOsInfo::hostOs(), repository.path(), filePath, relativeFilePath);
    if (arguments.isEmpty()) {
        if (!configuredCommand.trimmed().isEmpty()) {
            QMessageBox::warning(
                Core::ICore::dialogParent(),
                Tr::tr("Open in Git Client"),
                Tr::tr("Could not parse the command \"%1\".").arg(configuredCommand.trimmed()));
        }
        return;
    }
    CommandLine command(FilePath::fromUserInput(arguments.takeFirst()));
    command.addArgs(arguments);
    if (!Process::startDetached(command, repository)) {
        QMessageBox::warning(
            Core::ICore::dialogParent(),
            Tr::tr("Open in Git Client"),
            Tr::tr("Could not run \"%1\".").arg(command.toUserOutput()));
    }
}

void GitFileActions::openRepositoryInGitClient(const FilePath &repository)
{
    runGitClient(settings().gitClientRepositoryCommand(), repository);
}

void GitFileActions::openFileInGitClient(const FilePath &repository, const QString &relativePath)
{
    runGitClient(settings().gitClientFileCommand(), repository, relativePath);
}

} // namespace ChangesPanel

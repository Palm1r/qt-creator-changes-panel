// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "gitcommands.h"

#include "changespaneltr.h"

#include <git/gitclient.h>

#include <vcsbase/vcscommand.h>
#include <vcsbase/vcsenums.h>

using namespace Utils;

namespace ChangesPanel {

class GitPluginCommands final : public GitCommands
{
public:
    void requestStatus(const FilePath &repository, const StatusHandler &handler) override
    {
        VcsBase::VcsCommandData data;
        data.workingDirectory = repository;
        data.arguments = {"-c", "core.quotePath=false", "status", "-s", "--porcelain",
                          "--branch", "--untracked-files=all", "--ignore-submodules"};
        data.flags = VcsBase::RunFlag::NoOutput;
        data.commandHandler = [handler](const VcsBase::CommandResult &result) {
            if (result.result() == ProcessResult::FinishedWithSuccess) {
                handler({true, result.cleanedStdOut(), {}});
                return;
            }
            const QString error = result.cleanedStdErr().isEmpty() ? result.exitMessage()
                                                                   : result.cleanedStdErr();
            handler({false, {}, error});
        };
        Git::Internal::gitClient().enqueueCommand(data);
    }

    FilePath repositoryForDirectory(const FilePath &directory) const override
    {
        return Git::Internal::gitClient().findRepositoryForDirectory(directory);
    }

    FilePath gitDirForRepository(const FilePath &repository) const override
    {
        return Git::Internal::gitClient().findGitDirForRepository(repository);
    }

    QStringList submoduleStatusLines(const FilePath &repository) const override
    {
        return Git::Internal::gitClient().synchronousSubmoduleStatus(repository);
    }

    Result<> stageFiles(const FilePath &repository, const QStringList &relativePaths) override
    {
        if (Git::Internal::gitClient().synchronousAdd(repository, relativePaths))
            return ResultOk;
        return ResultError(Tr::tr("The git add command failed."));
    }

    Result<> unstageFiles(const FilePath &repository,
                          const QStringList &relativePaths,
                          const QStringList &intentToAddPaths) override
    {
        QString errorMessage;
        if (!Git::Internal::gitClient().synchronousReset(repository, relativePaths,
                                                         &errorMessage)) {
            return ResultError(errorMessage.isEmpty() ? Tr::tr("The git reset command failed.")
                                                      : errorMessage);
        }
        if (!intentToAddPaths.isEmpty()
            && !Git::Internal::gitClient().synchronousAdd(repository, intentToAddPaths,
                                                          {"--intent-to-add"})) {
            return ResultError(Tr::tr("The git add --intent-to-add command failed."));
        }
        return ResultOk;
    }

    Result<> checkoutFiles(const FilePath &repository, const QStringList &relativePaths) override
    {
        QString errorMessage;
        if (Git::Internal::gitClient()
                .synchronousCheckoutFiles(repository, relativePaths, {}, &errorMessage)) {
            return ResultOk;
        }
        if (errorMessage.isEmpty())
            errorMessage = Tr::tr("The git checkout command failed.");
        return ResultError(errorMessage);
    }

    void showDiff(const FilePath &repository, const QString &relativePath, bool staged) override
    {
        Git::Internal::gitClient().diffFile(
            repository,
            relativePath,
            staged ? Git::Internal::GitClient::Staged : Git::Internal::GitClient::Unstaged);
    }
};

GitCommands &gitCommands()
{
    static GitPluginCommands commands;
    return commands;
}

} // namespace ChangesPanel

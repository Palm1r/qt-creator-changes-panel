// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <utils/filepath.h>
#include <utils/result.h>

#include <QString>
#include <QStringList>

#include <functional>

namespace ChangesPanel {

class GitCommands
{
public:
    virtual ~GitCommands() = default;

    struct StatusResult
    {
        bool success = false;
        QString output;
        QString errorText;
    };
    using StatusHandler = std::function<void(const StatusResult &)>;

    virtual void requestStatus(const Utils::FilePath &repository, const StatusHandler &handler) = 0;

    virtual Utils::FilePath repositoryForDirectory(const Utils::FilePath &directory) const = 0;
    virtual Utils::FilePath gitDirForRepository(const Utils::FilePath &repository) const = 0;
    virtual QStringList submoduleStatusLines(const Utils::FilePath &repository) const = 0;

    virtual Utils::Result<> stageFiles(const Utils::FilePath &repository,
                                       const QStringList &relativePaths) = 0;
    virtual Utils::Result<> unstageFiles(const Utils::FilePath &repository,
                                         const QStringList &relativePaths,
                                         const QStringList &intentToAddPaths) = 0;
    virtual Utils::Result<> checkoutFiles(const Utils::FilePath &repository,
                                          const QStringList &relativePaths) = 0;
    virtual void showDiff(const Utils::FilePath &repository,
                          const QString &relativePath,
                          bool staged) = 0;
};

GitCommands &gitCommands();

} // namespace ChangesPanel

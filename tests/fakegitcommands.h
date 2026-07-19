// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include "gitcommands.h"

#include <utils/filepath.h>
#include <utils/result.h>

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace ChangesPanel {

class FakeGitCommands final : public GitCommands
{
public:
    struct PathsCall
    {
        Utils::FilePath repository;
        QStringList relativePaths;
        QStringList intentToAddPaths;
    };

    struct DiffCall
    {
        Utils::FilePath repository;
        QString relativePath;
        bool staged = false;
    };

    QList<PathsCall> stageCalls;
    QList<PathsCall> unstageCalls;
    QList<PathsCall> checkoutCalls;
    QList<DiffCall> diffCalls;

    QHash<Utils::FilePath, QString> failures;

    const PathsCall *stageCallFor(const Utils::FilePath &repository) const
    {
        return callFor(stageCalls, repository);
    }

    const PathsCall *unstageCallFor(const Utils::FilePath &repository) const
    {
        return callFor(unstageCalls, repository);
    }

    const PathsCall *checkoutCallFor(const Utils::FilePath &repository) const
    {
        return callFor(checkoutCalls, repository);
    }

    int mutationCount() const
    {
        return int(stageCalls.size() + unstageCalls.size() + checkoutCalls.size());
    }

    void requestStatus(const Utils::FilePath &, const StatusHandler &) override {}

    Utils::FilePath repositoryForDirectory(const Utils::FilePath &) const override { return {}; }

    Utils::FilePath gitDirForRepository(const Utils::FilePath &) const override { return {}; }

    QStringList submoduleStatusLines(const Utils::FilePath &) const override { return {}; }

    Utils::Result<> stageFiles(
        const Utils::FilePath &repository, const QStringList &relativePaths) override
    {
        stageCalls.append({repository, relativePaths, {}});
        return resultFor(repository);
    }

    Utils::Result<> unstageFiles(
        const Utils::FilePath &repository,
        const QStringList &relativePaths,
        const QStringList &intentToAddPaths) override
    {
        unstageCalls.append({repository, relativePaths, intentToAddPaths});
        return resultFor(repository);
    }

    Utils::Result<> checkoutFiles(
        const Utils::FilePath &repository, const QStringList &relativePaths) override
    {
        checkoutCalls.append({repository, relativePaths, {}});
        return resultFor(repository);
    }

    void showDiff(
        const Utils::FilePath &repository, const QString &relativePath, bool staged) override
    {
        diffCalls.append({repository, relativePath, staged});
    }

    void showDiffAll(
        const Utils::FilePath &, const QStringList &, const QStringList &) override
    {}

private:
    static const PathsCall *callFor(
        const QList<PathsCall> &calls, const Utils::FilePath &repository)
    {
        for (const PathsCall &call : calls) {
            if (call.repository == repository)
                return &call;
        }
        return nullptr;
    }

    Utils::Result<> resultFor(const Utils::FilePath &repository) const
    {
        const auto it = failures.constFind(repository);
        if (it == failures.cend())
            return Utils::ResultOk;
        return Utils::ResultError(*it);
    }
};

} // namespace ChangesPanel

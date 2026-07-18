// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include "gitstatusparser.h"

#include <utils/filepath.h>

#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QSet>

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

namespace Core {
class IDocument;
}

namespace ProjectExplorer {
class Project;
}

namespace Utils {
class FileSystemWatcher;
}

namespace ChangesPanel {

class GitCommands;

struct RepositoryInfo
{
    Utils::FilePath repository;
    Utils::FilePath parentRepository;
    bool isSubmodule = false;

    bool operator==(const RepositoryInfo &other) const = default;
};

class GitStatusTracker final : public QObject
{
    Q_OBJECT

public:
    explicit GitStatusTracker(GitCommands &git, QObject *parent = nullptr);

    void requestRefresh(const Utils::FilePath &repository);

    QList<Utils::FilePath> repositories() const;
    RepositoryInfo repositoryInfo(const Utils::FilePath &repository) const;

    void detachFromExternalSources();

signals:
    void statusChanged(const Utils::FilePath &repository, const GitStatus &status);
    void repositoryCleared(const Utils::FilePath &repository);
    void repositoriesChanged();
    void repositoryInfoChanged(const Utils::FilePath &repository);

private:
    bool isWatching(const Utils::FilePath &repository) const;
    void onProjectAdded(ProjectExplorer::Project *project);
    void onProjectRemoved(ProjectExplorer::Project *project);
    void onDocumentSaved(Core::IDocument *document);
    void onGitDirChanged(const Utils::FilePath &gitDir);
    void onVcsFileStatesChanged(const Utils::FilePath &repository);
    void onFileStatesCleared(const Utils::FilePath &repository);
    void onApplicationStateChanged(Qt::ApplicationState state);
    void updateGitDirWatches();
    void watchGitDir(const Utils::FilePath &gitDir, const Utils::FilePath &repository);
    void updateWatchFallbackTimer();
    void runStatusCommand(const Utils::FilePath &repository);
    void handleStatusFailure(const Utils::FilePath &repository, const QString &errorText);
    void publishStatus(const Utils::FilePath &repository, const GitStatus &status);
    QStringList submodulePathsFor(const Utils::FilePath &repository);

    GitCommands &m_git;
    Utils::FileSystemWatcher *m_gitDirWatcher = nullptr;
    QTimer *m_watchFallbackTimer = nullptr;
    QList<QMetaObject::Connection> m_externalConnections;
    QHash<Utils::FilePath, Utils::FilePath> m_repositoriesByGitDir;
    QHash<Utils::FilePath, Utils::FilePath> m_parentRepository;
    QHash<Utils::FilePath, GitStatus> m_lastStatus;
    QHash<Utils::FilePath, QStringList> m_submoduleCache;
    QSet<Utils::FilePath> m_pendingRefresh;
    QSet<Utils::FilePath> m_deferredRefresh;
    QSet<Utils::FilePath> m_polledRepositories;
    QHash<Utils::FilePath, int> m_statusRetries;
};

} // namespace ChangesPanel

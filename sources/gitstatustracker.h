// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <coreplugin/vcsmanager.h>

#include <utils/filepath.h>

#include <QHash>
#include <QObject>
#include <QSet>

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

struct GitStatus
{
    Core::FileStateHash fileStates;
    QSet<QString> stagedFiles;

    bool operator==(const GitStatus &other) const = default;
};

class GitStatusTracker final : public QObject
{
    Q_OBJECT

public:
    explicit GitStatusTracker(QObject *parent = nullptr);

    void requestRefresh(const Utils::FilePath &repository);
    bool isWatching(const Utils::FilePath &repository) const;

signals:
    void statusChanged(const Utils::FilePath &repository, const GitStatus &status);

private:
    void onProjectAdded(ProjectExplorer::Project *project);
    void onProjectRemoved(ProjectExplorer::Project *project);
    void onDocumentSaved(Core::IDocument *document);
    void onGitDirChanged(const Utils::FilePath &gitDir);
    void onFileStatesCleared(const Utils::FilePath &repository);
    void onApplicationStateChanged(Qt::ApplicationState state);
    void updateGitDirWatches();
    void runStatusCommand(const Utils::FilePath &repository);
    void publishStatus(const Utils::FilePath &repository, const GitStatus &status);

    Utils::FileSystemWatcher *m_gitDirWatcher = nullptr;
    QHash<Utils::FilePath, Utils::FilePath> m_repositoriesByGitDir;
    QHash<Utils::FilePath, GitStatus> m_lastStatus;
    QSet<Utils::FilePath> m_pendingRefresh;
    QSet<Utils::FilePath> m_deferredRefresh;
};

} // namespace ChangesPanel

// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "gitstatustracker.h"

#include "changespaneltr.h"
#include "gitcommands.h"

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/idocument.h>
#include <coreplugin/vcsmanager.h>

#include <vcsbase/vcsoutputwindow.h>

#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>

#include <utils/filesystemwatcher.h>

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QPointer>
#include <QTimer>

#include <algorithm>
#include <utility>

using namespace Core;
using namespace ProjectExplorer;
using namespace Utils;

namespace ChangesPanel {

static Q_LOGGING_CATEGORY(trackerLog, "qtc.changespanel.tracker", QtWarningMsg)

constexpr int kRefreshDebounceMs = 200;
constexpr int kMaxStatusRetries = 2;
constexpr int kWatchFallbackPollMs = 10000;

GitStatusTracker::GitStatusTracker(GitCommands &git, QObject *parent)
    : QObject(parent)
    , m_git(git)
    , m_gitDirWatcher(new FileSystemWatcher(this))
    , m_watchFallbackTimer(new QTimer(this))
{
    m_watchFallbackTimer->setInterval(kWatchFallbackPollMs);
    connect(m_watchFallbackTimer, &QTimer::timeout, this, [this] {
        for (const FilePath &repository : std::as_const(m_polledRepositories))
            requestRefresh(repository);
    });

    connect(
        m_gitDirWatcher,
        &FileSystemWatcher::directoryChanged,
        this,
        &GitStatusTracker::onGitDirChanged);
    m_externalConnections.append(connect(
        ProjectManager::instance(),
        &ProjectManager::projectAdded,
        this,
        &GitStatusTracker::onProjectAdded));
    m_externalConnections.append(connect(
        ProjectManager::instance(),
        &ProjectManager::projectRemoved,
        this,
        &GitStatusTracker::onProjectRemoved));
    m_externalConnections.append(connect(
        EditorManager::instance(),
        &EditorManager::saved,
        this,
        [this](IDocument *document, IDocument::SaveOption) { onDocumentSaved(document); }));
    m_externalConnections.append(connect(
        VcsManager::instance(),
        &VcsManager::updateFileState,
        this,
        &GitStatusTracker::onVcsFileStatesChanged));
    m_externalConnections.append(connect(
        VcsManager::instance(),
        &VcsManager::clearFileState,
        this,
        &GitStatusTracker::onFileStatesCleared));
    m_externalConnections.append(connect(
        qGuiApp,
        &QGuiApplication::applicationStateChanged,
        this,
        &GitStatusTracker::onApplicationStateChanged));

    for (Project *project : ProjectManager::projects())
        onProjectAdded(project);
}

void GitStatusTracker::detachFromExternalSources()
{
    for (const QMetaObject::Connection &connection : std::as_const(m_externalConnections))
        disconnect(connection);
    m_externalConnections.clear();
    m_watchFallbackTimer->stop();
}

void GitStatusTracker::requestRefresh(const FilePath &repository)
{
    if (qGuiApp->applicationState() != Qt::ApplicationActive) {
        m_deferredRefresh.insert(repository);
        return;
    }
    if (m_pendingRefresh.contains(repository))
        return;
    m_pendingRefresh.insert(repository);

    QTimer::singleShot(kRefreshDebounceMs, this, [this, repository] {
        m_pendingRefresh.remove(repository);
        if (qGuiApp->applicationState() != Qt::ApplicationActive) {
            m_deferredRefresh.insert(repository);
            return;
        }
        runStatusCommand(repository);
    });
}

void GitStatusTracker::onApplicationStateChanged(Qt::ApplicationState state)
{
    if (state != Qt::ApplicationActive)
        return;
    const QSet<FilePath> deferred = std::exchange(m_deferredRefresh, {});
    for (const FilePath &repository : deferred)
        requestRefresh(repository);
}

void GitStatusTracker::onVcsFileStatesChanged(const FilePath &repository)
{
    if (isWatching(repository))
        requestRefresh(repository);
}

void GitStatusTracker::onFileStatesCleared(const FilePath &repository)
{
    m_lastStatus.remove(repository);
    if (!isWatching(repository))
        return;
    emit repositoryCleared(repository);
    requestRefresh(repository);
}

bool GitStatusTracker::isWatching(const FilePath &repository) const
{
    return std::any_of(
        m_repositoriesByGitDir.cbegin(),
        m_repositoriesByGitDir.cend(),
        [&repository](const FilePath &watched) { return watched == repository; });
}

void GitStatusTracker::onProjectAdded(Project *project)
{
    if (!project)
        return;
    VcsManager::monitorDirectory(project->rootProjectDirectory(), true);
    updateGitDirWatches();
}

void GitStatusTracker::onProjectRemoved(Project *project)
{
    if (!project)
        return;
    VcsManager::monitorDirectory(project->rootProjectDirectory(), false);
    updateGitDirWatches();
}

void GitStatusTracker::onDocumentSaved(IDocument *document)
{
    if (!document || document->filePath().isEmpty())
        return;
    const FilePath repository = m_git.repositoryForDirectory(document->filePath().parentDir());
    if (!repository.isEmpty() && isWatching(repository))
        requestRefresh(repository);
}

void GitStatusTracker::onGitDirChanged(const FilePath &gitDir)
{
    const auto it = m_repositoriesByGitDir.constFind(gitDir);
    if (it != m_repositoriesByGitDir.cend()) {
        m_submoduleCache.remove(*it);
        requestRefresh(*it);
    }
}

void GitStatusTracker::watchGitDir(const FilePath &gitDir, const FilePath &repository)
{
    m_gitDirWatcher->addDirectory(gitDir, FileSystemWatcher::WatchAllChanges);
    if (m_gitDirWatcher->watchesDirectory(gitDir))
        return;
    qCWarning(trackerLog) << "Could not watch git dir" << gitDir.toUserOutput()
                          << "- falling back to polling for" << repository.toUserOutput();
    m_polledRepositories.insert(repository);
}

void GitStatusTracker::updateWatchFallbackTimer()
{
    if (m_polledRepositories.isEmpty())
        m_watchFallbackTimer->stop();
    else if (!m_watchFallbackTimer->isActive())
        m_watchFallbackTimer->start();
}

QStringList GitStatusTracker::submodulePathsFor(const FilePath &repository)
{
    const auto it = m_submoduleCache.constFind(repository);
    if (it != m_submoduleCache.cend())
        return *it;
    const QStringList paths = parseSubmoduleStatusLines(m_git.submoduleStatusLines(repository));
    m_submoduleCache.insert(repository, paths);
    return paths;
}

void GitStatusTracker::updateGitDirWatches()
{
    QHash<FilePath, FilePath> wanted;
    QHash<FilePath, FilePath> parents;
    for (Project *project : ProjectManager::projects()) {
        const FilePath repository = m_git.repositoryForDirectory(project->rootProjectDirectory());
        if (repository.isEmpty())
            continue;
        const FilePath gitDir = m_git.gitDirForRepository(repository);
        if (!gitDir.isEmpty())
            wanted.insert(gitDir, repository);
        const QStringList submodulePaths = submodulePathsFor(repository);
        for (const QString &submodulePath : submodulePaths) {
            const FilePath submodule = repository.pathAppended(submodulePath);
            const FilePath subGitDir = m_git.gitDirForRepository(submodule);
            if (subGitDir.isEmpty()) {
                qCWarning(trackerLog) << "Could not resolve git dir for submodule"
                                      << submodule.toUserOutput();
                continue;
            }
            if (wanted.contains(subGitDir))
                continue;
            wanted.insert(subGitDir, submodule);
            parents.insert(submodule, repository);
        }
    }
    const QHash<FilePath, FilePath> previousParents
        = std::exchange(m_parentRepository, parents);

    bool repositorySetChanged = false;
    for (auto it = m_repositoriesByGitDir.begin(); it != m_repositoriesByGitDir.end();) {
        if (wanted.contains(it.key())) {
            ++it;
            continue;
        }
        const FilePath repository = it.value();
        m_gitDirWatcher->removeDirectory(it.key());
        m_lastStatus.remove(repository);
        m_submoduleCache.remove(repository);
        m_deferredRefresh.remove(repository);
        m_polledRepositories.remove(repository);
        m_statusRetries.remove(repository);
        it = m_repositoriesByGitDir.erase(it);
        repositorySetChanged = true;
        emit repositoryCleared(repository);
    }
    for (auto it = wanted.cbegin(); it != wanted.cend(); ++it) {
        if (!m_repositoriesByGitDir.contains(it.key())) {
            m_repositoriesByGitDir.insert(it.key(), it.value());
            watchGitDir(it.key(), it.value());
            repositorySetChanged = true;
            requestRefresh(it.value());
        }
    }
    updateWatchFallbackTimer();

    if (repositorySetChanged)
        emit repositoriesChanged();

    for (const FilePath &repository : std::as_const(m_repositoriesByGitDir)) {
        if (previousParents.value(repository) != m_parentRepository.value(repository))
            emit repositoryInfoChanged(repository);
    }
}

void GitStatusTracker::runStatusCommand(const FilePath &repository)
{
    m_git.requestStatus(
        repository,
        [guard = QPointer(this), repository](const GitCommands::StatusResult &result) {
            if (!guard)
                return;
            if (!result.success) {
                guard->handleStatusFailure(repository, result.errorText);
                return;
            }
            guard->publishStatus(repository, parseStatusOutput(result.output));
        });
}

void GitStatusTracker::handleStatusFailure(const FilePath &repository, const QString &errorText)
{
    if (!isWatching(repository))
        return;
    m_lastStatus.remove(repository);
    const int attempts = m_statusRetries.value(repository);
    if (attempts >= kMaxStatusRetries) {
        qCWarning(trackerLog) << "git status failed" << (attempts + 1) << "times for"
                              << repository.toUserOutput() << ":" << errorText;
        VcsBase::VcsOutputWindow::appendError(
            repository,
            Tr::tr("Changes panel: git status failed for \"%1\": %2")
                .arg(repository.toUserOutput(),
                     errorText.isEmpty() ? Tr::tr("Unknown error.") : errorText));
        m_statusRetries.remove(repository);
        emit repositoryCleared(repository);
        return;
    }
    m_statusRetries.insert(repository, attempts + 1);
    requestRefresh(repository);
}

void GitStatusTracker::publishStatus(const FilePath &repository, const GitStatus &status)
{
    m_statusRetries.remove(repository);
    if (!isWatching(repository))
        return;
    const auto it = m_lastStatus.constFind(repository);
    if (it != m_lastStatus.cend() && *it == status)
        return;
    m_lastStatus.insert(repository, status);
    emit statusChanged(repository, status);
}

QList<FilePath> GitStatusTracker::repositories() const
{
    QList<FilePath> repos;
    for (const FilePath &repository : m_repositoriesByGitDir) {
        if (!repos.contains(repository))
            repos.append(repository);
    }
    std::sort(repos.begin(), repos.end());
    return repos;
}

RepositoryInfo GitStatusTracker::repositoryInfo(const FilePath &repository) const
{
    RepositoryInfo info;
    info.repository = repository;
    const auto it = m_parentRepository.constFind(repository);
    if (it != m_parentRepository.cend()) {
        info.isSubmodule = true;
        info.parentRepository = *it;
    }
    return info;
}

} // namespace ChangesPanel

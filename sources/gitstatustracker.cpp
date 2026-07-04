// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "gitstatustracker.h"

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/idocument.h>

#include <git/gitclient.h>

#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>

#include <utils/filesystemwatcher.h>

#include <vcsbase/vcsbaseclient.h>
#include <vcsbase/vcscommand.h>
#include <vcsbase/vcsenums.h>

#include <QPointer>
#include <QTimer>

#include <algorithm>

using namespace Core;
using namespace ProjectExplorer;
using namespace Utils;

namespace ChangesPanel {

constexpr int kRefreshDebounceMs = 200;

static VcsFileState stateForStatusChar(QChar statusChar)
{
    switch (statusChar.toLatin1()) {
    case 'M': return VcsFileState::Modified;
    case '?': return VcsFileState::Untracked;
    case 'A': return VcsFileState::Added;
    case 'R': return VcsFileState::Renamed;
    case 'D': return VcsFileState::Deleted;
    case 'U': return VcsFileState::Unmerged;
    default:  return VcsFileState::Unknown;
    }
}

static bool isRecordedInIndex(VcsFileState indexState)
{
    return indexState != VcsFileState::Unknown && indexState != VcsFileState::Untracked;
}

static GitStatus parseStatusOutput(const QString &output)
{
    GitStatus status;
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (line.size() <= 3)
            continue;

        const VcsFileState indexState = stateForStatusChar(line.at(0));
        const VcsFileState workTreeState = stateForStatusChar(line.at(1));
        const VcsFileState state = std::max(indexState, workTreeState);
        if (state == VcsFileState::Unknown)
            continue;

        QString relativePath = line.mid(3).trimmed();
        if (state == VcsFileState::Renamed) {
            const QStringList files = Git::Internal::gitClient().splitRenamedFilePattern(line);
            if (files.size() != 2)
                continue;
            relativePath = files.at(1);
        }

        status.fileStates.insert(relativePath, state);

        if (isRecordedInIndex(indexState) && state != VcsFileState::Unmerged)
            status.stagedFiles.insert(relativePath);
    }
    return status;
}

GitStatusTracker::GitStatusTracker(QObject *parent)
    : QObject(parent)
    , m_gitDirWatcher(new FileSystemWatcher(this))
{
    connect(
        m_gitDirWatcher,
        &FileSystemWatcher::directoryChanged,
        this,
        &GitStatusTracker::onGitDirChanged);
    connect(
        ProjectManager::instance(),
        &ProjectManager::projectAdded,
        this,
        &GitStatusTracker::onProjectAdded);
    connect(
        ProjectManager::instance(),
        &ProjectManager::projectRemoved,
        this,
        &GitStatusTracker::onProjectRemoved);
    connect(
        EditorManager::instance(),
        &EditorManager::saved,
        this,
        [this](IDocument *document, IDocument::SaveOption) { onDocumentSaved(document); });

    for (Project *project : ProjectManager::projects())
        onProjectAdded(project);
}

void GitStatusTracker::requestRefresh(const FilePath &repository)
{
    if (m_pendingRefresh.contains(repository))
        return;
    m_pendingRefresh.insert(repository);

    QTimer::singleShot(kRefreshDebounceMs, this, [this, repository] {
        m_pendingRefresh.remove(repository);
        runStatusCommand(repository);
    });
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
    VcsManager::monitorDirectory(project->rootProjectDirectory(), true);
    updateGitDirWatches();
}

void GitStatusTracker::onProjectRemoved(Project *project)
{
    VcsManager::monitorDirectory(project->rootProjectDirectory(), false);
    updateGitDirWatches();
}

void GitStatusTracker::onDocumentSaved(IDocument *document)
{
    if (!document || document->filePath().isEmpty())
        return;
    const FilePath repository
        = Git::Internal::gitClient().findRepositoryForDirectory(document->filePath().parentDir());
    if (!repository.isEmpty() && isWatching(repository))
        requestRefresh(repository);
}

void GitStatusTracker::onGitDirChanged(const FilePath &gitDir)
{
    const auto it = m_repositoriesByGitDir.constFind(gitDir);
    if (it != m_repositoriesByGitDir.cend())
        requestRefresh(*it);
}

void GitStatusTracker::updateGitDirWatches()
{
    using Git::Internal::gitClient;

    QHash<FilePath, FilePath> wanted;
    for (Project *project : ProjectManager::projects()) {
        const FilePath repository
            = gitClient().findRepositoryForDirectory(project->rootProjectDirectory());
        if (repository.isEmpty())
            continue;
        const FilePath gitDir = gitClient().findGitDirForRepository(repository);
        if (!gitDir.isEmpty())
            wanted.insert(gitDir, repository);
    }

    for (auto it = m_repositoriesByGitDir.begin(); it != m_repositoriesByGitDir.end();) {
        if (wanted.contains(it.key())) {
            ++it;
        } else {
            m_gitDirWatcher->removeDirectory(it.key());
            it = m_repositoriesByGitDir.erase(it);
        }
    }
    for (auto it = wanted.cbegin(); it != wanted.cend(); ++it) {
        if (!m_repositoriesByGitDir.contains(it.key())) {
            m_repositoriesByGitDir.insert(it.key(), it.value());
            m_gitDirWatcher->addDirectory(it.key(), FileSystemWatcher::WatchAllChanges);
        }
    }
}

void GitStatusTracker::runStatusCommand(const FilePath &repository)
{
    VcsBase::VcsCommandData data;
    data.workingDirectory = repository;
    data.arguments
        = {"-c", "core.quotePath=false", "status", "-s", "--porcelain", "--ignore-submodules"};
    data.flags = VcsBase::RunFlag::NoOutput;
    data.commandHandler
        = [guard = QPointer(this), repository](const VcsBase::CommandResult &result) {
              if (!guard)
                  return;
              emit guard->statusChanged(repository, parseStatusOutput(result.cleanedStdOut()));
          };
    Git::Internal::gitClient().enqueueCommand(data);
}

} // namespace ChangesPanel

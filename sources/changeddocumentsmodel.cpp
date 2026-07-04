// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "changeddocumentsmodel.h"

#include "changespaneltr.h"
#include "gitfileactions.h"
#include "gitstatustracker.h"

#include <coreplugin/vcsmanager.h>

#include <projectexplorer/projectmanager.h>

#include <utils/fsengine/fileiconprovider.h>
#include <utils/stringutils.h>
#include <utils/treemodel.h>

#include <QFont>
#include <QTimer>

#include <algorithm>

using namespace Core;
using namespace Utils;

namespace ChangesPanel {

constexpr quintptr kGroupInternalId = ChangedDocumentsModel::GroupCount;

ChangedDocumentsModel::ChangedDocumentsModel(GitStatusTracker *tracker, QObject *parent)
    : QAbstractItemModel(parent)
    , m_tracker(tracker)
{
    connect(
        VcsManager::instance(),
        &VcsManager::updateFileState,
        this,
        &ChangedDocumentsModel::onVcsFileStatesChanged);
    connect(
        VcsManager::instance(),
        &VcsManager::clearFileState,
        this,
        &ChangedDocumentsModel::clearRepository);
    connect(
        m_tracker,
        &GitStatusTracker::statusChanged,
        this,
        &ChangedDocumentsModel::onStatusChanged);
    connect(
        ProjectExplorer::ProjectManager::instance(),
        &ProjectExplorer::ProjectManager::projectRemoved,
        this,
        &ChangedDocumentsModel::scheduleRevalidate);
}

QModelIndex ChangedDocumentsModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column < 0 || column >= ColumnCount)
        return {};
    if (!parent.isValid()) {
        if (row < 0 || row >= GroupCount)
            return {};
        return createIndex(row, column, kGroupInternalId);
    }
    if (parent.internalId() != kGroupInternalId || parent.column() != 0)
        return {};
    const int group = parent.row();
    if (row < 0 || row >= int(m_entries.at(group).size()))
        return {};
    return createIndex(row, column, quintptr(group));
}

QModelIndex ChangedDocumentsModel::parent(const QModelIndex &child) const
{
    if (!child.isValid() || child.internalId() == kGroupInternalId)
        return {};
    return createIndex(int(child.internalId()), 0, kGroupInternalId);
}

QModelIndex ChangedDocumentsModel::groupIndex(int group) const
{
    return index(group, 0);
}

int ChangedDocumentsModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return GroupCount;
    if (parent.internalId() == kGroupInternalId && parent.column() == 0)
        return int(m_entries.at(parent.row()).size());
    return 0;
}

int ChangedDocumentsModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return ColumnCount;
}

QVariant ChangedDocumentsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};
    if (index.internalId() == kGroupInternalId)
        return groupData(index.row(), index.column(), role);

    const int group = int(index.internalId());
    if (index.row() < 0 || index.row() >= int(m_entries.at(group).size()))
        return {};
    return entryData(m_entries.at(group).at(index.row()), group, index.column(), role);
}

QVariant ChangedDocumentsModel::groupData(int group, int column, int role) const
{
    if (role == Qt::DisplayRole && column == FileNameColumn) {
        switch (group) {
        case MergeGroup:    return Tr::tr("Merge Changes");
        case StagedGroup:   return Tr::tr("Staged Changes");
        case UnstagedGroup: return Tr::tr("Unstaged Changes");
        }
    }
    return {};
}

QVariant ChangedDocumentsModel::entryData(const Entry &entry, int group, int column,
                                          int role) const
{
    switch (role) {
    case Qt::DisplayRole:
        if (column == FileNameColumn)
            return entry.filePath.fileName();
        return {};
    case Qt::DecorationRole:
        if (column == FileNameColumn)
            return FileIconProvider::icon(entry.filePath);
        return {};
    case Qt::ForegroundRole:
        if (column == FileNameColumn)
            return VcsManager::fileStateColor(entry.state);
        return {};
    case Qt::FontRole:
        if (entry.state == VcsFileState::Deleted) {
            QFont font;
            font.setStrikeOut(true);
            return font;
        }
        return {};
    case Qt::ToolTipRole: {
        if (column == DiffColumn)
            return Tr::tr("Diff \"%1\"").arg(entry.filePath.fileName());
        if (column == RevertColumn) {
            if (entry.state == VcsFileState::Modified)
                return Tr::tr("Revert All Changes to \"%1\"").arg(entry.filePath.fileName());
            if (entry.state == VcsFileState::Deleted)
                return Tr::tr("Recover \"%1\"").arg(entry.filePath.fileName());
            return {};
        }
        if (column == StageColumn) {
            switch (stageActionFor(entry.state, group == StagedGroup)) {
            case StageAction::Stage:
                return Tr::tr("Stage \"%1\"").arg(entry.filePath.fileName());
            case StageAction::Unstage:
                return Tr::tr("Unstage \"%1\"").arg(entry.filePath.fileName());
            case StageAction::None:
                return {};
            }
            return {};
        }
        QString toolTip = entry.filePath.toUserOutput();
        const QString description = VcsManager::fileStateDescription(entry.state);
        if (!description.isEmpty())
            toolTip += "<p>" + description;
        return toolTip;
    }
    case FilePathRole:
        return entry.filePath.toVariant();
    case StateRole:
        return int(entry.state);
    case StagedRole:
        return group == StagedGroup;
    case RelativeDirectoryRole:
        if (column == FileNameColumn)
            return entry.relativeDirectory;
        return {};
    }
    return {};
}

bool ChangedDocumentsModel::lessThan(const Entry &a, const Entry &b)
{
    const int result = caseFriendlyCompare(a.filePath.fileName(), b.filePath.fileName());
    if (result != 0)
        return result < 0;
    return a.filePath < b.filePath;
}

ChangedDocumentsModel::EntryLocation ChangedDocumentsModel::findEntry(
    const FilePath &filePath) const
{
    Entry probe;
    probe.filePath = filePath;
    for (int group = 0; group < GroupCount; ++group) {
        const auto &entries = m_entries.at(group);
        const auto it = std::lower_bound(entries.cbegin(), entries.cend(), probe, &lessThan);
        if (it != entries.cend() && it->filePath == filePath)
            return {group, int(it - entries.cbegin())};
    }
    return {};
}

int ChangedDocumentsModel::groupFor(
    const FilePath &filePath, const FilePath &repository, VcsFileState state) const
{
    if (state == VcsFileState::Unmerged)
        return MergeGroup;
    const QString relativePath = filePath.relativeChildPath(repository).path();
    return m_stagedFiles.value(repository).contains(relativePath) ? StagedGroup : UnstagedGroup;
}

bool ChangedDocumentsModel::tracksRepository(const FilePath &repository) const
{
    for (int group = 0; group < GroupCount; ++group) {
        const bool found = std::any_of(
            m_entries.at(group).cbegin(), m_entries.at(group).cend(),
            [&repository](const Entry &entry) { return entry.repository == repository; });
        if (found)
            return true;
    }
    return false;
}

void ChangedDocumentsModel::onStatusChanged(const FilePath &repository, const GitStatus &status)
{
    if (!m_tracker->isWatching(repository) && !tracksRepository(repository))
        return;

    applyStagedFiles(repository, status.stagedFiles);
    removeVanishedEntries(repository, status.fileStates);
    for (auto it = status.fileStates.cbegin(); it != status.fileStates.cend(); ++it) {
        const QString &relativePath = it.key();
        if (relativePath.isEmpty() || relativePath.endsWith('/'))
            continue;
        const FilePath filePath = repository.pathAppended(relativePath);
        const QString relativeDir = filePath.parentDir().relativeChildPath(repository).path();
        setState(filePath, repository, relativeDir, it.value());
    }
}

void ChangedDocumentsModel::removeVanishedEntries(const FilePath &repository,
                                                  const Core::FileStateHash &states)
{
    for (int group = 0; group < GroupCount; ++group) {
        for (int i = int(m_entries.at(group).size()) - 1; i >= 0; --i) {
            const Entry &entry = m_entries.at(group).at(i);
            if (entry.repository != repository)
                continue;
            const QString relativePath = entry.filePath.relativeChildPath(repository).path();
            if (!states.contains(relativePath))
                removeEntry(group, i);
        }
    }
}

void ChangedDocumentsModel::onVcsFileStatesChanged(const FilePath &repository)
{
    m_tracker->requestRefresh(repository);
}

void ChangedDocumentsModel::setState(
    const FilePath &filePath,
    const FilePath &repository,
    const QString &relativeDirectory,
    VcsFileState state)
{
    const EntryLocation location = findEntry(filePath);
    if (state == VcsFileState::Unknown) {
        if (location.isValid())
            removeEntry(location.group, location.row);
        return;
    }
    const int targetGroup = groupFor(filePath, repository, state);
    if (!location.isValid()) {
        insertEntry(targetGroup, {filePath, repository, relativeDirectory, state});
        return;
    }
    if (m_entries.at(location.group).at(location.row).state == state)
        return;
    if (location.group == targetGroup) {
        m_entries.at(location.group)[location.row].state = state;
        emit dataChanged(
            index(location.row, 0, groupIndex(location.group)),
            index(location.row, ColumnCount - 1, groupIndex(location.group)),
            {Qt::ForegroundRole, Qt::FontRole, Qt::ToolTipRole, StateRole});
    } else {
        Entry entry = m_entries.at(location.group).at(location.row);
        entry.state = state;
        removeEntry(location.group, location.row);
        insertEntry(targetGroup, std::move(entry));
    }
}

void ChangedDocumentsModel::clearRepository(const FilePath &repository)
{
    m_stagedFiles.remove(repository);
    for (int group = 0; group < GroupCount; ++group) {
        for (int i = int(m_entries.at(group).size()) - 1; i >= 0; --i) {
            if (m_entries.at(group).at(i).repository == repository)
                removeEntry(group, i);
        }
    }
}

void ChangedDocumentsModel::scheduleRevalidate()
{
    QTimer::singleShot(0, this, &ChangedDocumentsModel::revalidate);
}

void ChangedDocumentsModel::revalidate()
{
    for (int group = 0; group < GroupCount; ++group) {
        for (int i = int(m_entries.at(group).size()) - 1; i >= 0; --i) {
            if (!m_tracker->isWatching(m_entries.at(group).at(i).repository))
                removeEntry(group, i);
        }
    }
    for (auto it = m_stagedFiles.begin(); it != m_stagedFiles.end();) {
        if (m_tracker->isWatching(it.key()))
            ++it;
        else
            it = m_stagedFiles.erase(it);
    }
}

void ChangedDocumentsModel::removeEntry(int group, int row)
{
    beginRemoveRows(groupIndex(group), row, row);
    m_entries.at(group).erase(m_entries.at(group).begin() + row);
    endRemoveRows();
}

void ChangedDocumentsModel::insertEntry(int group, Entry entry)
{
    auto &entries = m_entries.at(group);
    const auto it = std::lower_bound(entries.cbegin(), entries.cend(), entry, &lessThan);
    const int newRow = int(it - entries.cbegin());
    beginInsertRows(groupIndex(group), newRow, newRow);
    entries.insert(it, std::move(entry));
    endInsertRows();
}

void ChangedDocumentsModel::applyStagedFiles(const FilePath &repository,
                                             const QSet<QString> &stagedFiles)
{
    if (m_stagedFiles.value(repository) == stagedFiles)
        return;
    m_stagedFiles.insert(repository, stagedFiles);

    QList<FilePath> changed;
    for (int group : {int(StagedGroup), int(UnstagedGroup)}) {
        for (const Entry &entry : m_entries.at(group)) {
            if (entry.repository != repository)
                continue;
            const QString relativePath = entry.filePath.relativeChildPath(repository).path();
            if ((group == StagedGroup) != stagedFiles.contains(relativePath))
                changed.append(entry.filePath);
        }
    }
    for (const FilePath &filePath : changed) {
        const EntryLocation location = findEntry(filePath);
        if (!location.isValid())
            continue;
        Entry entry = m_entries.at(location.group).at(location.row);
        removeEntry(location.group, location.row);
        insertEntry(location.group == StagedGroup ? UnstagedGroup : StagedGroup,
                    std::move(entry));
    }
}

} // namespace ChangesPanel

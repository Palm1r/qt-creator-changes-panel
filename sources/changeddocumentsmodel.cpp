// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "changeddocumentsmodel.h"

#include "changespaneltr.h"
#include "gitfileactions.h"
#include "gitstatustracker.h"

#include <coreplugin/vcsmanager.h>

#include <utils/fsengine/fileiconprovider.h>
#include <utils/qtcassert.h>
#include <utils/stringutils.h>
#include <utils/treemodel.h>

#include <QFont>

#include <algorithm>


using namespace Core;
using namespace Utils;

namespace ChangesPanel {

constexpr quintptr kGroupInternalId = ChangedDocumentsModel::GroupCount;

static VcsFileState toCoreState(FileState state)
{
    switch (state) {
    case FileState::Untracked: return VcsFileState::Untracked;
    case FileState::Added:     return VcsFileState::Added;
    case FileState::Modified:  return VcsFileState::Modified;
    case FileState::Deleted:   return VcsFileState::Deleted;
    case FileState::Renamed:   return VcsFileState::Renamed;
    case FileState::Unmerged:  return VcsFileState::Unmerged;
    case FileState::Unknown:   break;
    }
    return VcsFileState::Unknown;
}

struct GroupInfo
{
    QString (*title)();
    StageAction stageAction;
};

static const GroupInfo &groupInfo(int group)
{
    static const GroupInfo infos[ChangedDocumentsModel::GroupCount] = {
        {[] { return Tr::tr("Merge Changes"); }, StageAction::None},
        {[] { return Tr::tr("Staged Changes"); }, StageAction::Unstage},
        {[] { return Tr::tr("Unstaged Changes"); }, StageAction::Stage},
    };
    return infos[group];
}

ChangedDocumentsModel::ChangedDocumentsModel(GitStatusTracker *tracker, QObject *parent)
    : QAbstractItemModel(parent)
{
    connect(
        tracker,
        &GitStatusTracker::statusChanged,
        this,
        &ChangedDocumentsModel::onStatusChanged);
    connect(
        tracker,
        &GitStatusTracker::repositoryCleared,
        this,
        &ChangedDocumentsModel::clearRepository);
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

QModelIndex ChangedDocumentsModel::indexForFile(const FilePath &filePath) const
{
    const EntryLocation location = findEntry(filePath);
    if (!location.isValid())
        return {};
    return index(location.row, FileNameColumn, groupIndex(location.group));
}

bool ChangedDocumentsModel::hasRevertableEntries(int group, const FilePath &repository) const
{
    if (group < 0 || group >= GroupCount)
        return false;
    const auto &entries = m_entries.at(group);
    return std::any_of(entries.cbegin(), entries.cend(), [&repository](const Entry &entry) {
        return isRevertable(entry.state)
               && (repository.isEmpty() || entry.repository == repository);
    });
}

QVariant ChangedDocumentsModel::groupData(int group, int column, int role) const
{
    if (role == Qt::DisplayRole && column == FileNameColumn)
        return groupInfo(group).title();
    if (role == GroupStageActionRole)
        return int(groupInfo(group).stageAction);
    if (role == GroupHasRevertableRole)
        return hasRevertableEntries(group, {});
    if (role == Qt::ToolTipRole) {
        if (column == RevertColumn)
            return Tr::tr("Revert All Changes in \"%1\"").arg(groupInfo(group).title());
        if (column == StageColumn) {
            switch (groupInfo(group).stageAction) {
            case StageAction::Stage:   return Tr::tr("Stage All Changes");
            case StageAction::Unstage: return Tr::tr("Unstage All Changes");
            case StageAction::None:    break;
            }
        }
        return {};
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
            return VcsManager::fileStateColor(toCoreState(entry.state));
        return {};
    case Qt::FontRole:
        if (entry.state == FileState::Deleted) {
            QFont font;
            font.setStrikeOut(true);
            return font;
        }
        return {};
    case Qt::ToolTipRole:
        return toolTipData(entry, group, column);
    case FilePathRole:
        return entry.filePath.toVariant();
    case StateRole:
        return int(entry.state);
    case StagedRole:
        return group == StagedGroup;
    case RepositoryRole:
        return entry.repository.toVariant();
    case RelativePathRole:
        return entry.relativePath;
    case RelativeDirectoryRole:
        if (column == FileNameColumn)
            return entry.relativeDirectory;
        return {};
    }
    return {};
}

QVariant ChangedDocumentsModel::toolTipData(const Entry &entry, int group, int column) const
{
    if (column == DiffColumn) {
        const QString toolTip = fileEntryActionToolTip(
            diffColumnActionFor(entry.state), entry.filePath.fileName());
        return toolTip.isEmpty() ? QVariant() : QVariant(toolTip);
    }
    if (column == RevertColumn) {
        if (entry.state == FileState::Modified)
            return Tr::tr("Revert All Changes to \"%1\"").arg(entry.filePath.fileName());
        if (entry.state == FileState::Deleted)
            return Tr::tr("Recover \"%1\"").arg(entry.filePath.fileName());
        return {};
    }
    if (column == StageColumn) {
        const QString toolTip = stageActionToolTip(
            stageActionFor(entry.state, group == StagedGroup), entry.filePath.fileName());
        return toolTip.isEmpty() ? QVariant() : QVariant(toolTip);
    }
    QString toolTip = entry.filePath.toUserOutput();
    const QString description = VcsManager::fileStateDescription(toCoreState(entry.state));
    if (!description.isEmpty())
        toolTip += "<p>" + description;
    return toolTip;
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
    const QString &relativePath, const FilePath &repository, FileState state) const
{
    if (state == FileState::Unmerged)
        return MergeGroup;
    const auto it = m_stagedFiles.constFind(repository);
    return it != m_stagedFiles.cend() && it->contains(relativePath) ? StagedGroup
                                                                    : UnstagedGroup;
}

void ChangedDocumentsModel::onStatusChanged(const FilePath &repository, const GitStatus &status)
{
    applyStagedFiles(repository, status.stagedFiles);
    removeVanishedEntries(repository, status.fileStates);
    for (auto it = status.fileStates.cbegin(); it != status.fileStates.cend(); ++it) {
        const QString &relativePath = it.key();
        if (relativePath.isEmpty() || relativePath.endsWith('/')
            || relativePath.startsWith("../")) {
            continue;
        }
        const FilePath filePath = repository.pathAppended(relativePath);
        const QString relativeDir = filePath.parentDir().relativeChildPath(repository).path();
        setState({filePath, repository, relativePath, relativeDir, it.value()});
    }
}

void ChangedDocumentsModel::removeVanishedEntries(const FilePath &repository,
                                                  const FileStateMap &states)
{
    removeEntriesIf([&repository, &states](const Entry &entry) {
        return entry.repository == repository && !states.contains(entry.relativePath);
    });
}

void ChangedDocumentsModel::setState(Entry entry)
{
    QTC_ASSERT(entry.state != FileState::Unknown, return);
    const EntryLocation location = findEntry(entry.filePath);
    const int targetGroup = groupFor(entry.relativePath, entry.repository, entry.state);
    if (!location.isValid()) {
        insertEntry(targetGroup, std::move(entry));
        return;
    }
    if (location.group == targetGroup) {
        const FileState previousState = m_entries.at(location.group).at(location.row).state;
        if (previousState == entry.state)
            return;
        m_entries.at(location.group)[location.row].state = entry.state;
        emit dataChanged(
            index(location.row, 0, groupIndex(location.group)),
            index(location.row, ColumnCount - 1, groupIndex(location.group)),
            {Qt::ForegroundRole, Qt::FontRole, Qt::ToolTipRole, StateRole});
        if (isRevertable(previousState) != isRevertable(entry.state)) {
            emit dataChanged(
                groupIndex(location.group),
                index(location.group, ColumnCount - 1),
                {GroupHasRevertableRole});
        }
    } else {
        Entry moved = m_entries.at(location.group).at(location.row);
        moved.state = entry.state;
        removeEntry(location.group, location.row);
        insertEntry(targetGroup, std::move(moved));
    }
}

void ChangedDocumentsModel::clearRepository(const FilePath &repository)
{
    m_stagedFiles.remove(repository);
    removeEntriesIf(
        [&repository](const Entry &entry) { return entry.repository == repository; });
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

template<typename Predicate>
void ChangedDocumentsModel::removeEntriesIf(Predicate predicate)
{
    for (int group = 0; group < GroupCount; ++group) {
        auto &entries = m_entries.at(group);
        int end = int(entries.size());
        while (end > 0) {
            if (!predicate(entries.at(end - 1))) {
                --end;
                continue;
            }
            int begin = end - 1;
            while (begin > 0 && predicate(entries.at(begin - 1)))
                --begin;
            beginRemoveRows(groupIndex(group), begin, end - 1);
            entries.erase(entries.begin() + begin, entries.begin() + end);
            endRemoveRows();
            end = begin;
        }
    }
}

void ChangedDocumentsModel::applyStagedFiles(const FilePath &repository,
                                             const QSet<QString> &stagedFiles)
{
    const auto it = m_stagedFiles.constFind(repository);
    const bool unchanged = it != m_stagedFiles.cend() ? *it == stagedFiles
                                                      : stagedFiles.isEmpty();
    if (unchanged)
        return;
    m_stagedFiles.insert(repository, stagedFiles);

    QList<FilePath> changed;
    for (int group : {int(StagedGroup), int(UnstagedGroup)}) {
        for (const Entry &entry : m_entries.at(group)) {
            if (entry.repository != repository)
                continue;
            if ((group == StagedGroup) != stagedFiles.contains(entry.relativePath))
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

bool isGroupHeader(const QModelIndex &index)
{
    return filePathAt(index).isEmpty();
}

FilePath filePathAt(const QModelIndex &index)
{
    return FilePath::fromVariant(index.data(FilePathRole));
}

FilePath repositoryAt(const QModelIndex &index)
{
    return FilePath::fromVariant(index.data(ChangedDocumentsModel::RepositoryRole));
}

QString relativePathAt(const QModelIndex &index)
{
    return index.data(ChangedDocumentsModel::RelativePathRole).toString();
}

FileState fileStateAt(const QModelIndex &index)
{
    return FileState(index.data(ChangedDocumentsModel::StateRole).toInt());
}

bool stagedAt(const QModelIndex &index)
{
    return index.data(ChangedDocumentsModel::StagedRole).toBool();
}

StageAction groupStageActionAt(const QModelIndex &index)
{
    return StageAction(index.data(ChangedDocumentsModel::GroupStageActionRole).toInt());
}

bool groupHasRevertableAt(const QModelIndex &index)
{
    return index.data(ChangedDocumentsModel::GroupHasRevertableRole).toBool();
}

} // namespace ChangesPanel

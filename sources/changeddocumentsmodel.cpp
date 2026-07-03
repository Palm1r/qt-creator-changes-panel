// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "changeddocumentsmodel.h"

#include <coreplugin/vcsmanager.h>

#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>

#include <utils/fsengine/fileiconprovider.h>
#include <utils/stringutils.h>
#include <utils/theme/theme.h>
#include <utils/treemodel.h>

#include <QFont>
#include <QTimer>

using namespace Core;
using namespace Utils;

namespace ChangesPanel {

ChangedDocumentsModel::ChangedDocumentsModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    connect(
        VcsManager::instance(),
        &VcsManager::updateFileState,
        this,
        &ChangedDocumentsModel::updateFileStates);
    connect(
        VcsManager::instance(),
        &VcsManager::clearFileState,
        this,
        &ChangedDocumentsModel::clearRepository);

    using namespace ProjectExplorer;
    connect(
        ProjectManager::instance(),
        &ProjectManager::projectAdded,
        this,
        &ChangedDocumentsModel::onProjectAdded);
    connect(
        ProjectManager::instance(),
        &ProjectManager::projectRemoved,
        this,
        &ChangedDocumentsModel::onProjectRemoved);

    for (Project *project : ProjectManager::projects())
        onProjectAdded(project);
}

void ChangedDocumentsModel::onProjectAdded(ProjectExplorer::Project *project)
{
    VcsManager::monitorDirectory(project->rootProjectDirectory(), true);
}

void ChangedDocumentsModel::onProjectRemoved(ProjectExplorer::Project *project)
{
    VcsManager::monitorDirectory(project->rootProjectDirectory(), false);
    QTimer::singleShot(0, this, &ChangedDocumentsModel::revalidate);
}

int ChangedDocumentsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

int ChangedDocumentsModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant ChangedDocumentsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= int(m_entries.size()))
        return {};

    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return index.column() == FileNameColumn ? entry.filePath.fileName()
                                                : entry.relativeDirectory;
    case Qt::DecorationRole:
        if (index.column() == FileNameColumn)
            return FileIconProvider::icon(entry.filePath);
        return {};
    case Qt::ForegroundRole:
        if (index.column() == FileNameColumn)
            return VcsManager::fileStateColor(entry.state);
        return creatorColor(Theme::TextColorDisabled);
    case Qt::TextAlignmentRole:
        if (index.column() == DirectoryColumn)
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        return {};
    case Qt::FontRole:
        if (entry.state == VcsFileState::Deleted) {
            QFont font;
            font.setStrikeOut(true);
            return font;
        }
        return {};
    case Qt::ToolTipRole: {
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

int ChangedDocumentsModel::rowOf(const FilePath &filePath) const
{
    // m_entries is kept sorted by lessThan(), which orders solely by filePath,
    // so an existing entry can be located with a binary search.
    Entry probe;
    probe.filePath = filePath;
    const auto it = std::lower_bound(m_entries.cbegin(), m_entries.cend(), probe, &lessThan);
    if (it != m_entries.cend() && it->filePath == filePath)
        return int(it - m_entries.cbegin());
    return -1;
}

void ChangedDocumentsModel::updateFileStates(const FilePath &repository, const QStringList &files)
{
    for (const QString &relativePath : files) {
        if (relativePath.isEmpty())
            continue;
        const FilePath filePath = repository.pathAppended(relativePath);
        const QString relativeDir = filePath.parentDir().relativeChildPath(repository).path();
        setState(filePath, repository, relativeDir, VcsManager::fileState(filePath));
    }
}

void ChangedDocumentsModel::setState(
    const FilePath &filePath,
    const FilePath &repository,
    const QString &relativeDir,
    VcsFileState state)
{
    const int row = rowOf(filePath);
    if (state == VcsFileState::Unknown) {
        if (row >= 0)
            removeEntry(row);
        return;
    }
    if (row >= 0) {
        if (m_entries[row].state != state) {
            m_entries[row].state = state;
            emit dataChanged(
                index(row, 0),
                index(row, ColumnCount - 1),
                {Qt::ForegroundRole, Qt::FontRole, Qt::ToolTipRole, StateRole});
        }
        return;
    }
    const Entry entry{filePath, repository, relativeDir, state};
    const auto it = std::lower_bound(m_entries.cbegin(), m_entries.cend(), entry, &lessThan);
    const int newRow = int(it - m_entries.cbegin());
    beginInsertRows({}, newRow, newRow);
    m_entries.insert(it, entry);
    endInsertRows();
}

void ChangedDocumentsModel::clearRepository(const FilePath &repository)
{
    for (int i = int(m_entries.size()) - 1; i >= 0; --i) {
        if (m_entries.at(i).repository == repository)
            removeEntry(i);
    }
}

void ChangedDocumentsModel::revalidate()
{
    for (int i = int(m_entries.size()) - 1; i >= 0; --i) {
        const Entry &entry = m_entries.at(i);
        const VcsFileState state = VcsManager::fileState(entry.filePath);
        if (state == VcsFileState::Unknown)
            removeEntry(i);
        else if (m_entries[i].state != state)
            setState(entry.filePath, entry.repository, entry.relativeDirectory, state);
    }
}

void ChangedDocumentsModel::removeEntry(int row)
{
    beginRemoveRows({}, row, row);
    m_entries.erase(m_entries.begin() + row);
    endRemoveRows();
}

} // namespace ChangesPanel

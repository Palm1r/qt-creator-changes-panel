// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include "gitstatusparser.h"

#include <utils/filepath.h>

#include <QAbstractItemModel>
#include <QHash>
#include <QSet>

#include <array>
#include <vector>

namespace ChangesPanel {

enum class StageAction;

class ChangedDocumentsModel final : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Role : int {
        StateRole = Qt::UserRole + 1,
        StagedRole,
        RelativeDirectoryRole,
        GroupStageActionRole,
        GroupHasRevertableRole,
        RepositoryRole,
        RelativePathRole,
    };
    enum Column : int {
        FileNameColumn,
        GitClientColumn,
        RevertColumn,
        DiffColumn,
        StageColumn,
        ColumnCount,
    };
    enum Group : int {
        MergeGroup,
        StagedGroup,
        UnstagedGroup,
        GroupCount,
    };

    explicit ChangedDocumentsModel(QObject *parent = nullptr);

    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const final;
    QModelIndex parent(const QModelIndex &child) const final;
    int rowCount(const QModelIndex &parent = {}) const final;
    int columnCount(const QModelIndex &parent = {}) const final;
    QVariant data(const QModelIndex &index, int role) const final;

    QModelIndex indexForFile(const Utils::FilePath &filePath) const;
    bool hasRevertableEntries(int group, const Utils::FilePath &repository) const;

    void applyStatus(const Utils::FilePath &repository, const GitStatus &status);
    void clearRepository(const Utils::FilePath &repository);

private:
    struct Entry
    {
        Utils::FilePath filePath;
        Utils::FilePath repository;
        QString relativePath;
        QString relativeDirectory;
        FileState state = FileState::Unknown;
    };
    struct EntryLocation
    {
        int group = -1;
        int row = -1;
        bool isValid() const { return group >= 0; }
    };

    QModelIndex groupIndex(int group) const;
    QVariant groupData(int group, int column, int role) const;
    QVariant entryData(const Entry &entry, int group, int column, int role) const;
    QVariant toolTipData(const Entry &entry, int group, int column) const;

    static bool lessThan(const Entry &a, const Entry &b);
    EntryLocation findEntry(const Utils::FilePath &filePath) const;
    int groupFor(
        const QString &relativePath,
        const Utils::FilePath &repository,
        FileState state) const;

    void removeVanishedEntries(
        const Utils::FilePath &repository, const FileStateMap &states);
    void setState(Entry entry);
    void removeEntry(int group, int row);
    void insertEntry(int group, Entry entry);
    template<typename Predicate> void removeEntriesIf(Predicate predicate);
    void applyStagedFiles(const Utils::FilePath &repository, const QSet<QString> &stagedFiles);

    std::array<std::vector<Entry>, GroupCount> m_entries;
    QHash<Utils::FilePath, QSet<QString>> m_stagedFiles;
};

bool isGroupHeader(const QModelIndex &index);
Utils::FilePath filePathAt(const QModelIndex &index);
Utils::FilePath repositoryAt(const QModelIndex &index);
QString relativePathAt(const QModelIndex &index);
FileState fileStateAt(const QModelIndex &index);
bool stagedAt(const QModelIndex &index);
StageAction groupStageActionAt(const QModelIndex &index);
bool groupHasRevertableAt(const QModelIndex &index);

} // namespace ChangesPanel

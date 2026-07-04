// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <coreplugin/vcsmanager.h>

#include <utils/filepath.h>

#include <QAbstractItemModel>
#include <QHash>
#include <QSet>

#include <array>
#include <vector>

namespace ChangesPanel {

class GitStatusTracker;
struct GitStatus;

class ChangedDocumentsModel final : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Role : int {
        StateRole = Qt::UserRole + 1,
        StagedRole,
        RelativeDirectoryRole,
    };
    enum Column : int {
        FileNameColumn,
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

    explicit ChangedDocumentsModel(GitStatusTracker *tracker, QObject *parent = nullptr);

    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const final;
    QModelIndex parent(const QModelIndex &child) const final;
    int rowCount(const QModelIndex &parent = {}) const final;
    int columnCount(const QModelIndex &parent = {}) const final;
    QVariant data(const QModelIndex &index, int role) const final;

private:
    struct Entry
    {
        Utils::FilePath filePath;
        Utils::FilePath repository;
        QString relativeDirectory;
        Core::VcsFileState state = Core::VcsFileState::Unknown;
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

    static bool lessThan(const Entry &a, const Entry &b);
    EntryLocation findEntry(const Utils::FilePath &filePath) const;
    int groupFor(
        const Utils::FilePath &filePath,
        const Utils::FilePath &repository,
        Core::VcsFileState state) const;
    bool tracksRepository(const Utils::FilePath &repository) const;

    void onStatusChanged(const Utils::FilePath &repository, const GitStatus &status);
    void removeVanishedEntries(
        const Utils::FilePath &repository, const Core::FileStateHash &states);
    void onVcsFileStatesChanged(const Utils::FilePath &repository);
    void setState(
        const Utils::FilePath &filePath,
        const Utils::FilePath &repository,
        const QString &relativeDirectory,
        Core::VcsFileState state);
    void clearRepository(const Utils::FilePath &repository);
    void scheduleRevalidate();
    void revalidate();
    void removeEntry(int group, int row);
    void insertEntry(int group, Entry entry);
    void applyStagedFiles(const Utils::FilePath &repository, const QSet<QString> &stagedFiles);

    GitStatusTracker *m_tracker = nullptr;
    std::array<std::vector<Entry>, GroupCount> m_entries;
    QHash<Utils::FilePath, QSet<QString>> m_stagedFiles;
};

} // namespace ChangesPanel

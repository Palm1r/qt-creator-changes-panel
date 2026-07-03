// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <coreplugin/vcsfilestate.h>

#include <utils/filepath.h>

#include <QAbstractTableModel>

namespace ProjectExplorer {
class Project;
}

namespace ChangesPanel {

class ChangedDocumentsModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Role : int {
        StateRole = Qt::UserRole + 1,
    };
    enum Column : int {
        FileNameColumn,
        DirectoryColumn,
        ColumnCount,
    };

    explicit ChangedDocumentsModel(QObject *parent = nullptr);

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

    static bool lessThan(const Entry &a, const Entry &b);
    int rowOf(const Utils::FilePath &filePath) const;

    void onProjectAdded(ProjectExplorer::Project *project);
    void onProjectRemoved(ProjectExplorer::Project *project);

    void updateFileStates(const Utils::FilePath &repository, const QStringList &files);
    void setState(
        const Utils::FilePath &filePath,
        const Utils::FilePath &repository,
        const QString &relativeDir,
        Core::VcsFileState state);
    void clearRepository(const Utils::FilePath &repository);
    void revalidate();
    void removeEntry(int row);

    std::vector<Entry> m_entries;
};

} // namespace ChangesPanel

// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <utils/filepath.h>

#include <QSortFilterProxyModel>

namespace ChangesPanel {

class ChangedDocumentsProxyModel final : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setSourceModel(QAbstractItemModel *sourceModel) final;
    QVariant data(const QModelIndex &index, int role) const final;

    void setRepositoryFilter(const Utils::FilePath &repository);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const final;

private:
    bool acceptsFile(int sourceRow, const QModelIndex &sourceParent) const;
    void scheduleRefilter();

    QList<QMetaObject::Connection> m_sourceConnections;
    bool m_refilterPending = false;
    Utils::FilePath m_repository;
};

} // namespace ChangesPanel

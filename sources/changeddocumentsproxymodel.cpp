// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "changeddocumentsproxymodel.h"

#include "changeddocumentsmodel.h"

#include <QTimer>

#include <utility>

namespace ChangesPanel {

void ChangedDocumentsProxyModel::setSourceModel(QAbstractItemModel *sourceModel)
{
    for (const QMetaObject::Connection &connection : std::as_const(m_sourceConnections))
        disconnect(connection);
    m_sourceConnections.clear();

    QSortFilterProxyModel::setSourceModel(sourceModel);
    if (!sourceModel)
        return;

    m_sourceConnections.append(connect(sourceModel, &QAbstractItemModel::rowsInserted,
                                       this, &ChangedDocumentsProxyModel::scheduleRefilter));
    m_sourceConnections.append(connect(sourceModel, &QAbstractItemModel::rowsRemoved,
                                       this, &ChangedDocumentsProxyModel::scheduleRefilter));
}

QVariant ChangedDocumentsProxyModel::data(const QModelIndex &index, int role) const
{
    if (role == ChangedDocumentsModel::GroupHasRevertableRole && !m_repository.isEmpty()) {
        const auto *model = qobject_cast<ChangedDocumentsModel *>(sourceModel());
        const QModelIndex source = mapToSource(index);
        if (model && source.isValid() && !source.parent().isValid())
            return model->hasRevertableEntries(source.row(), m_repository);
    }
    return QSortFilterProxyModel::data(index, role);
}

void ChangedDocumentsProxyModel::scheduleRefilter()
{
    if (m_refilterPending)
        return;
    m_refilterPending = true;
    QTimer::singleShot(0, this, [this] {
        m_refilterPending = false;
        invalidateRowsFilter();
    });
}

void ChangedDocumentsProxyModel::setRepositoryFilter(const Utils::FilePath &repository)
{
    if (m_repository == repository)
        return;
    m_repository = repository;
    invalidateRowsFilter();
}

bool ChangedDocumentsProxyModel::filterAcceptsRow(int sourceRow,
                                                  const QModelIndex &sourceParent) const
{
    if (!sourceParent.isValid()) {
        const QModelIndex group = sourceModel()->index(sourceRow, 0);
        const int count = sourceModel()->rowCount(group);
        for (int row = 0; row < count; ++row) {
            if (acceptsFile(row, group))
                return true;
        }
        return false;
    }
    return acceptsFile(sourceRow, sourceParent);
}

bool ChangedDocumentsProxyModel::acceptsFile(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    return m_repository.isEmpty() || repositoryAt(idx) == m_repository;
}

} // namespace ChangesPanel

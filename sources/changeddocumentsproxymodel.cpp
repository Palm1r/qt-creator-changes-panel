// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "changeddocumentsproxymodel.h"

#include "changeddocumentsmodel.h"

#include <QTimer>

namespace ChangesPanel {

void ChangedDocumentsProxyModel::setSourceModel(QAbstractItemModel *sourceModel)
{
    QSortFilterProxyModel::setSourceModel(sourceModel);

    const auto refilterGroupsAfterSourceChange = [this] {
        QTimer::singleShot(0, this, &ChangedDocumentsProxyModel::invalidateRowsFilter);
    };
    connect(sourceModel, &QAbstractItemModel::rowsInserted,
            this, refilterGroupsAfterSourceChange);
    connect(sourceModel, &QAbstractItemModel::rowsRemoved,
            this, refilterGroupsAfterSourceChange);
}

void ChangedDocumentsProxyModel::setShowUntracked(bool show)
{
    if (m_showUntracked == show)
        return;
    m_showUntracked = show;
    invalidateRowsFilter();
}

bool ChangedDocumentsProxyModel::showUntracked() const
{
    return m_showUntracked;
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
    if (m_showUntracked)
        return true;
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    return Core::VcsFileState(idx.data(ChangedDocumentsModel::StateRole).toInt())
           != Core::VcsFileState::Untracked;
}

} // namespace ChangesPanel

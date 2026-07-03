// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "changeddocumentsproxymodel.h"

#include "changeddocumentsmodel.h"

namespace ChangesPanel {

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

bool ChangedDocumentsProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_showUntracked)
        return true;
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    return Core::VcsFileState(idx.data(ChangedDocumentsModel::StateRole).toInt())
           != Core::VcsFileState::Untracked;
}

} // namespace ChangesPanel

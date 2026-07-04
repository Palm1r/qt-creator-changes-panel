// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <QSortFilterProxyModel>

namespace ChangesPanel {

class ChangedDocumentsProxyModel final : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setSourceModel(QAbstractItemModel *sourceModel) final;

    void setShowUntracked(bool show);
    bool showUntracked() const;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const final;

private:
    bool acceptsFile(int sourceRow, const QModelIndex &sourceParent) const;

    bool m_showUntracked = true;
};

} // namespace ChangesPanel

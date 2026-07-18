// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <QPersistentModelIndex>
#include <QStyledItemDelegate>

namespace ChangesPanel {

constexpr int kTrailingPadding = 10;

class ChangedDocumentsDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void setHoveredIndex(const QModelIndex &index);

private:
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const final;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const final;
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const final;

    void paintRelativeDirectory(QPainter *painter, const QStyleOptionViewItem &option,
                                const QModelIndex &index) const;
    void paintActionButton(QPainter *painter, const QStyleOptionViewItem &option,
                           const QModelIndex &index) const;
    void paintGroupActionButton(QPainter *painter, const QStyleOptionViewItem &option,
                                const QModelIndex &index) const;

    QPersistentModelIndex m_hoveredIndex;
};

} // namespace ChangesPanel

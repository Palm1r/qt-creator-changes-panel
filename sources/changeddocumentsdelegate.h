// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include "changeddocumentsmodel.h"
#include "gitfileactions.h"

#include <QFontMetrics>
#include <QIcon>
#include <QModelIndex>
#include <QPoint>
#include <QRect>
#include <QStyledItemDelegate>
#include <QVarLengthArray>

namespace ChangesPanel {

constexpr int kTrailingPadding = 10;

struct RowActionZone
{
    ChangedDocumentsModel::Column column = ChangedDocumentsModel::ColumnCount;
    QRect rect;
};

struct RowActionZones
{
    QVarLengthArray<RowActionZone, 4> zones;
    bool groupHeader = false;
    FileState state = FileState::Unknown;
    bool staged = false;
    StageAction groupStageAction = StageAction::None;

    const RowActionZone *zoneAt(const QPoint &pos) const;
};

RowActionZones rowActionZones(
    const QRect &rowRect, const QFontMetrics &metrics, const QModelIndex &index);

const QIcon &diffActionIcon();

class ChangedDocumentsDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void setHoverPosition(const QPoint &pos);
    void clearHoverPosition();

    bool helpEvent(QHelpEvent *event, QAbstractItemView *view,
                   const QStyleOptionViewItem &option, const QModelIndex &index) final;

private:
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const final;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const final;
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const final;

    void paintRelativeDirectory(QPainter *painter, const QStyleOptionViewItem &option,
                                const QModelIndex &index) const;
    void paintActionOverlay(QPainter *painter, const QStyleOptionViewItem &option,
                            const QModelIndex &index) const;

    static constexpr QPoint kNoHoverPosition{-1, -1};

    QPoint m_hoverPosition = kNoHoverPosition;
};

} // namespace ChangesPanel

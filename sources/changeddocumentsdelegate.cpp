// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "changeddocumentsdelegate.h"

#include "changeddocumentsmodel.h"
#include "changespanelsettings.h"
#include "gitfileactions.h"

#include <utils/icon.h>
#include <utils/theme/theme.h>
#include <utils/utilsicons.h>

#include <QAbstractItemView>
#include <QApplication>
#include <QHelpEvent>
#include <QPainter>
#include <QToolTip>

using namespace Utils;

namespace ChangesPanel {

constexpr int kDirectoryGap = 8;
constexpr int kDirectoryRightPadding = 2;
constexpr int kMinDirectoryWidth = 12;
constexpr int kGroupHeaderExtraHeight = 4;
constexpr int kActionIconMargin = 6;
constexpr int kOverlayFadeWidth = 14;
constexpr int kOverlayAlpha = 222;

const QIcon &diffActionIcon()
{
    static const QIcon icon
        = Icon({{":/diffeditor/images/sidebysidediff.png", Theme::IconsBaseColor}}, Icon::Tint)
              .icon();
    return icon;
}

static const QIcon &revertActionIcon()
{
    static const QIcon icon = Icons::UNDO.icon();
    return icon;
}

static const QIcon &gitClientActionIcon()
{
    static const QIcon icon
        = Icon({{":/changespanel/icons/gitclient.png", Theme::IconsBaseColor}}, Icon::Tint)
              .icon();
    return icon;
}

static QIcon stageActionIcon(StageAction action)
{
    static const QIcon stageIcon = Icons::PLUS.icon();
    static const QIcon unstageIcon = Icons::MINUS.icon();
    switch (action) {
    case StageAction::Stage:   return stageIcon;
    case StageAction::Unstage: return unstageIcon;
    case StageAction::None:    break;
    }
    return {};
}

static QIcon actionIcon(ChangedDocumentsModel::Column column, FileState state, bool staged)
{
    static const QIcon openIcon = Icons::OPENFILE.icon();
    switch (column) {
    case ChangedDocumentsModel::DiffColumn:
        switch (diffColumnActionFor(state)) {
        case FileEntryAction::OpenEditor: return openIcon;
        case FileEntryAction::ShowDiff:   return diffActionIcon();
        case FileEntryAction::None:       return {};
        }
        return {};
    case ChangedDocumentsModel::RevertColumn:
        return isRevertable(state) ? revertActionIcon() : QIcon();
    case ChangedDocumentsModel::StageColumn:
        return stageActionIcon(stageActionFor(state, staged));
    case ChangedDocumentsModel::GitClientColumn:
        return gitClientActionIcon();
    case ChangedDocumentsModel::FileNameColumn:
    case ChangedDocumentsModel::ColumnCount:
        break;
    }
    return {};
}

static QIcon zoneIcon(const RowActionZones &row, ChangedDocumentsModel::Column column)
{
    if (row.groupHeader) {
        if (column == ChangedDocumentsModel::RevertColumn)
            return revertActionIcon();
        return stageActionIcon(row.groupStageAction);
    }
    return actionIcon(column, row.state, row.staged);
}

const RowActionZone *RowActionZones::zoneAt(const QPoint &pos) const
{
    for (const RowActionZone &zone : zones) {
        if (zone.rect.contains(pos))
            return &zone;
    }
    return nullptr;
}

RowActionZones rowActionZones(
    const QRect &rowRect, const QFontMetrics &metrics, const QModelIndex &index)
{
    RowActionZones row;
    row.groupHeader = isGroupHeader(index);
    QVarLengthArray<ChangedDocumentsModel::Column, 4> columns;
    if (row.groupHeader) {
        row.groupStageAction = groupStageActionAt(index);
        if (settings().showRevertButton() && groupHasRevertableAt(index))
            columns.append(ChangedDocumentsModel::RevertColumn);
        if (settings().showStageButton() && row.groupStageAction != StageAction::None)
            columns.append(ChangedDocumentsModel::StageColumn);
    } else {
        row.state = fileStateAt(index);
        row.staged = stagedAt(index);
        if (settings().showGitClientButton() && settings().hasGitClientFileCommand())
            columns.append(ChangedDocumentsModel::GitClientColumn);
        if (settings().showRevertButton() && isRevertable(row.state))
            columns.append(ChangedDocumentsModel::RevertColumn);
        if (settings().showDiffButton()
            && diffColumnActionFor(row.state) != FileEntryAction::None) {
            columns.append(ChangedDocumentsModel::DiffColumn);
        }
        if (settings().showStageButton()
            && stageActionFor(row.state, row.staged) != StageAction::None) {
            columns.append(ChangedDocumentsModel::StageColumn);
        }
    }

    const int extent = metrics.height() + kActionIconMargin;
    const int right = rowRect.right() - kTrailingPadding;
    const int count = int(columns.size());
    for (int i = 0; i < count; ++i) {
        const int left = right - (count - i) * extent + 1;
        if (left < rowRect.left())
            continue;
        row.zones.append(
            {columns.at(i), QRect(left, rowRect.top(), extent, rowRect.height())});
    }
    return row;
}

void ChangedDocumentsDelegate::setHoverPosition(const QPoint &pos)
{
    m_hoverPosition = pos;
}

void ChangedDocumentsDelegate::clearHoverPosition()
{
    m_hoverPosition = kNoHoverPosition;
}

bool ChangedDocumentsDelegate::helpEvent(QHelpEvent *event, QAbstractItemView *view,
                                         const QStyleOptionViewItem &option,
                                         const QModelIndex &index)
{
    if (event->type() == QEvent::ToolTip && index.isValid()) {
        const RowActionZones row = rowActionZones(option.rect, option.fontMetrics, index);
        if (const RowActionZone *zone = row.zoneAt(event->pos())) {
            const QString text
                = index.siblingAtColumn(zone->column).data(Qt::ToolTipRole).toString();
            if (!text.isEmpty()) {
                QToolTip::showText(event->globalPos(), text, view);
                return true;
            }
        }
    }
    return QStyledItemDelegate::helpEvent(event, view, option, index);
}

void ChangedDocumentsDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                     const QModelIndex &index) const
{
    if (isGroupHeader(index)) {
        QStyledItemDelegate::paint(painter, option, index);
        if (option.state & QStyle::State_MouseOver)
            paintActionOverlay(painter, option, index);
        return;
    }

    const bool hovered = option.state & QStyle::State_MouseOver;
    if (hovered)
        painter->fillRect(option.rect, option.palette.alternateBase());

    QStyledItemDelegate::paint(painter, option, index);

    if (index.column() == ChangedDocumentsModel::FileNameColumn)
        paintRelativeDirectory(painter, option, index);
    if (hovered)
        paintActionOverlay(painter, option, index);
}

QSize ChangedDocumentsDelegate::sizeHint(const QStyleOptionViewItem &option,
                                         const QModelIndex &index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    if (isGroupHeader(index))
        size.setHeight(size.height() + kGroupHeaderExtraHeight);
    return size;
}

void ChangedDocumentsDelegate::initStyleOption(QStyleOptionViewItem *option,
                                               const QModelIndex &index) const
{
    QStyledItemDelegate::initStyleOption(option, index);
    option->palette.setColor(QPalette::HighlightedText, option->palette.color(QPalette::Text));
    if (index.column() == ChangedDocumentsModel::FileNameColumn && isGroupHeader(index)) {
        const int count = index.model()->rowCount(index.siblingAtColumn(0));
        option->text += QStringLiteral(" (%1)").arg(count);
    }
}

void ChangedDocumentsDelegate::paintRelativeDirectory(QPainter *painter,
                                                      const QStyleOptionViewItem &option,
                                                      const QModelIndex &index) const
{
    const QString dir = index.data(ChangedDocumentsModel::RelativeDirectoryRole).toString();
    if (dir.isEmpty())
        return;

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    const QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
    const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);

    const QFontMetrics metrics(opt.font);
    const int x = textRect.left() + metrics.horizontalAdvance(opt.text) + kDirectoryGap;
    const int available = textRect.right() - x - kDirectoryRightPadding;
    if (available <= kMinDirectoryWidth)
        return;

    painter->save();
    painter->setFont(opt.font);
    painter->setPen(creatorColor(Theme::TextColorDisabled));
    painter->drawText(QRect(x, textRect.top(), available, textRect.height()),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      metrics.elidedText(dir, Qt::ElideRight, available));
    painter->restore();
}

void ChangedDocumentsDelegate::paintActionOverlay(QPainter *painter,
                                                  const QStyleOptionViewItem &option,
                                                  const QModelIndex &index) const
{
    const RowActionZones row = rowActionZones(option.rect, option.fontMetrics, index);
    if (row.zones.isEmpty())
        return;

    QColor backdrop;
    if (option.state & QStyle::State_Selected)
        backdrop = option.palette.color(QPalette::Highlight);
    else if (row.groupHeader)
        backdrop = option.palette.color(QPalette::Base);
    else
        backdrop = option.palette.color(QPalette::AlternateBase);

    QColor solid = backdrop;
    solid.setAlpha(kOverlayAlpha);
    QColor clear = backdrop;
    clear.setAlpha(0);

    const int stripLeft = row.zones.first().rect.left();
    const QRect fadeRect(stripLeft - kOverlayFadeWidth, option.rect.top(),
                         kOverlayFadeWidth, option.rect.height());
    QLinearGradient fade(fadeRect.topLeft(), fadeRect.topRight());
    fade.setColorAt(0, clear);
    fade.setColorAt(1, solid);
    painter->fillRect(fadeRect.intersected(option.rect), fade);
    painter->fillRect(QRect(stripLeft, option.rect.top(),
                            option.rect.right() - stripLeft + 1, option.rect.height()),
                      solid);

    for (const RowActionZone &zone : row.zones) {
        if (zone.rect.contains(m_hoverPosition))
            painter->fillRect(zone.rect, option.palette.mid());
        zoneIcon(row, zone.column).paint(painter, zone.rect, Qt::AlignCenter);
    }
}

} // namespace ChangesPanel

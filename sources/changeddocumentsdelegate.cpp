// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "changeddocumentsdelegate.h"

#include "changeddocumentsmodel.h"
#include "gitfileactions.h"

#include <utils/filepath.h>
#include <utils/icon.h>
#include <utils/theme/theme.h>
#include <utils/treemodel.h>
#include <utils/utilsicons.h>

#include <QApplication>
#include <QPainter>

using namespace Core;
using namespace Utils;

namespace ChangesPanel {

constexpr int kDirectoryGap = 8;
constexpr int kGroupHeaderExtraHeight = 4;

static bool isGroupHeader(const QModelIndex &index)
{
    return FilePath::fromVariant(index.data(FilePathRole)).isEmpty();
}

static const QIcon &actionIcon(int column, VcsFileState state, bool staged)
{
    static const QIcon diffIcon
        = Icon({{":/diffeditor/images/sidebysidediff.png", Theme::IconsBaseColor}}, Icon::Tint)
              .icon();
    static const QIcon stageIcon = Icons::PLUS.icon();
    static const QIcon unstageIcon = Icons::MINUS.icon();
    static const QIcon revertIcon = Icons::UNDO.icon();
    static const QIcon noIcon;
    switch (column) {
    case ChangedDocumentsModel::DiffColumn:
        return diffIcon;
    case ChangedDocumentsModel::RevertColumn:
        return isRevertable(state) ? revertIcon : noIcon;
    case ChangedDocumentsModel::StageColumn:
        switch (stageActionFor(state, staged)) {
        case StageAction::Stage:   return stageIcon;
        case StageAction::Unstage: return unstageIcon;
        case StageAction::None:    return noIcon;
        }
        return noIcon;
    default:
        return noIcon;
    }
}

void ChangedDocumentsDelegate::setHoveredIndex(const QModelIndex &index)
{
    m_hoveredIndex = index;
}

void ChangedDocumentsDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                     const QModelIndex &index) const
{
    if (isGroupHeader(index)) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    const bool hovered = option.state & QStyle::State_MouseOver;
    if (hovered)
        painter->fillRect(option.rect, option.palette.alternateBase());

    QStyledItemDelegate::paint(painter, option, index);

    if (index.column() == ChangedDocumentsModel::FileNameColumn)
        paintRelativeDirectory(painter, option, index);
    else if (hovered)
        paintActionButton(painter, option, index);
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
    const int available = textRect.right() - x - 2;
    if (available <= 12)
        return;

    painter->save();
    painter->setFont(opt.font);
    painter->setPen(creatorColor(Theme::TextColorDisabled));
    painter->drawText(QRect(x, textRect.top(), available, textRect.height()),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      metrics.elidedText(dir, Qt::ElideRight, available));
    painter->restore();
}

void ChangedDocumentsDelegate::paintActionButton(QPainter *painter,
                                                 const QStyleOptionViewItem &option,
                                                 const QModelIndex &index) const
{
    const auto state = VcsFileState(index.data(ChangedDocumentsModel::StateRole).toInt());
    const bool staged = index.data(ChangedDocumentsModel::StagedRole).toBool();
    const QIcon &icon = actionIcon(index.column(), state, staged);
    if (icon.isNull())
        return;

    QRect buttonRect = option.rect;
    if (index.column() == ChangedDocumentsModel::StageColumn)
        buttonRect.adjust(0, 0, -kTrailingPadding, 0);

    if (index == m_hoveredIndex)
        painter->fillRect(buttonRect, option.palette.mid());
    icon.paint(painter, buttonRect, Qt::AlignCenter);
}

} // namespace ChangesPanel

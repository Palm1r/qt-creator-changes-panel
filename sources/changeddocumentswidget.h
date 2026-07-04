// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <QWidget>

QT_BEGIN_NAMESPACE
class QLabel;
class QToolButton;
QT_END_NAMESPACE

namespace Utils {
class TreeView;
}

namespace ChangesPanel {

class ChangedDocumentsModel;
class ChangedDocumentsProxyModel;

class ChangedDocumentsWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ChangedDocumentsWidget(ChangedDocumentsModel *model);

    QToolButton *createMenuButton();

private:
    void contextMenuRequested(const QPoint &pos);
    void updateEmptyState();

    Utils::TreeView *m_view = nullptr;
    ChangedDocumentsProxyModel *m_proxy = nullptr;
    QLabel *m_emptyLabel = nullptr;
};

} // namespace ChangesPanel

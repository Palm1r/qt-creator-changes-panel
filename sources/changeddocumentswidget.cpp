// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "changeddocumentswidget.h"

#include "changeddocumentsmodel.h"
#include "changeddocumentsproxymodel.h"
#include "changespanelconstants.h"
#include "changespaneltr.h"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/fileutils.h>
#include <coreplugin/icore.h>

#include <utils/filepath.h>
#include <utils/itemviews.h>
#include <utils/stringutils.h>
#include <utils/stylehelper.h>
#include <utils/icon.h>
#include <utils/theme/theme.h>
#include <utils/treemodel.h>

#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Core;
using namespace Utils;

namespace ChangesPanel {

ChangedDocumentsWidget::ChangedDocumentsWidget(ChangedDocumentsModel *model)
{
    setWindowTitle(Tr::tr("Changes"));

    m_proxy = new ChangedDocumentsProxyModel(this);
    m_proxy->setSourceModel(model);
    m_proxy->setShowUntracked(
        ICore::settings()->value(Constants::SHOW_UNTRACKED_KEY, true).toBool());

    m_view = new TreeView(this);
    setFocusProxy(m_view);
    m_view->setModel(m_proxy);
    m_view->setRootIsDecorated(false);
    m_view->setHeaderHidden(true);
    m_view->header()
        ->setSectionResizeMode(ChangedDocumentsModel::FileNameColumn, QHeaderView::ResizeToContents);
    m_view->setUniformRowHeights(true);
    m_view->setTextElideMode(Qt::ElideMiddle);
    m_view->setFrameStyle(QFrame::NoFrame);
    m_view->setAttribute(Qt::WA_MacShowFocusRect, false);
    m_view->setActivationMode(SingleClickActivation);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);

    m_emptyLabel = new QLabel(
        Tr::tr(
            "No changed files.\n\nFile states are provided by the version control "
            "system for the open projects. Make sure \"Show file status\" is enabled "
            "in Preferences > Version Control > General."),
        this);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_emptyLabel->setContentsMargins(8, 8, 8, 8);
    m_emptyLabel->setEnabled(false);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_emptyLabel);
    layout->addWidget(m_view);

    connect(
        m_proxy, &QAbstractItemModel::rowsInserted, this, &ChangedDocumentsWidget::updateEmptyState);
    connect(
        m_proxy, &QAbstractItemModel::rowsRemoved, this, &ChangedDocumentsWidget::updateEmptyState);
    connect(m_proxy, &QAbstractItemModel::modelReset, this, &ChangedDocumentsWidget::updateEmptyState);
    updateEmptyState();

    connect(m_view, &QAbstractItemView::activated, this, [](const QModelIndex &index) {
        const auto state = Core::VcsFileState(index.data(ChangedDocumentsModel::StateRole).toInt());
        if (state == Core::VcsFileState::Deleted)
            return;
        const auto filePath = FilePath::fromVariant(index.data(FilePathRole));
        if (!filePath.isEmpty())
            EditorManager::openEditor(filePath);
    });
    connect(
        m_view,
        &QWidget::customContextMenuRequested,
        this,
        &ChangedDocumentsWidget::contextMenuRequested);
}

QToolButton *ChangedDocumentsWidget::createMenuButton()
{
    auto button = new QToolButton;
    static const QIcon menuIcon
        = Icon({{":/changespanel/icons/morevert.png", Theme::IconsBaseColor}}, Icon::Tint).icon();
    button->setIcon(menuIcon);
    button->setToolTip(Tr::tr("Changes Menu"));
    button->setPopupMode(QToolButton::InstantPopup);
    button->setProperty(StyleHelper::C_NO_ARROW, true);

    auto menu = new QMenu(button);

    QAction *showUntracked = menu->addAction(Tr::tr("Show Untracked Files"));
    showUntracked->setCheckable(true);
    showUntracked->setChecked(m_proxy->showUntracked());
    connect(showUntracked, &QAction::toggled, this, [this](bool checked) {
        m_proxy->setShowUntracked(checked);
        ICore::settings()->setValue(Constants::SHOW_UNTRACKED_KEY, checked);
    });

    if (ActionContainer *gitContainer = ActionManager::actionContainer(Utils::Id("Git"))) {
        if (QMenu *gitMenu = gitContainer->menu(); gitMenu && !gitMenu->actions().isEmpty()) {
            menu->addSeparator();
            menu->addActions(gitMenu->actions());
        }
    }

    button->setMenu(menu);
    return button;
}

void ChangedDocumentsWidget::updateEmptyState()
{
    m_emptyLabel->setVisible(m_proxy->rowCount() == 0);
}

void ChangedDocumentsWidget::contextMenuRequested(const QPoint &pos)
{
    const QModelIndex index = m_view->indexAt(pos);
    if (!index.isValid())
        return;
    const auto filePath = FilePath::fromVariant(index.data(FilePathRole));
    const auto state = Core::VcsFileState(index.data(ChangedDocumentsModel::StateRole).toInt());

    QMenu menu;
    QAction *openAction = menu.addAction(Tr::tr("Open"), this, [filePath] {
        EditorManager::openEditor(filePath);
    });
    openAction->setEnabled(state != Core::VcsFileState::Deleted);
    menu.addAction(Tr::tr("Open Containing Folder"), this, [filePath] {
        Core::FileUtils::showInGraphicalShell(filePath);
    });
    menu.addAction(Tr::tr("Copy Full Path"), this, [filePath] {
        setClipboardAndSelection(filePath.toUserOutput());
    });
    menu.exec(m_view->mapToGlobal(pos));
}

} // namespace ChangesPanel

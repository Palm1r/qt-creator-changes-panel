// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "changeddocumentswidget.h"

#include "changeddocumentsdelegate.h"
#include "changeddocumentsmodel.h"
#include "changeddocumentsproxymodel.h"
#include "changespanelconstants.h"
#include "changespaneltr.h"
#include "gitfileactions.h"
#include "gitstatustracker.h"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/fileutils.h>
#include <coreplugin/icore.h>
#include <coreplugin/idocument.h>
#include <coreplugin/iversioncontrol.h>
#include <coreplugin/vcsmanager.h>

#include <utils/filepath.h>
#include <utils/icon.h>
#include <utils/itemviews.h>
#include <utils/stringutils.h>
#include <utils/stylehelper.h>
#include <utils/theme/theme.h>
#include <utils/treemodel.h>

#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Core;
using namespace Utils;

namespace ChangesPanel {

constexpr int kActionIconMargin = 6;
constexpr int kViewIndentation = 12;

ChangedDocumentsWidget::ChangedDocumentsWidget(ChangedDocumentsModel *model,
                                               GitStatusTracker *tracker)
    : m_tracker(tracker)
{
    setWindowTitle(Tr::tr("Changes"));

    m_proxy = new ChangedDocumentsProxyModel(this);
    m_proxy->setSourceModel(model);
    m_proxy->setShowUntracked(
        ICore::settings()->value(Constants::SHOW_UNTRACKED_KEY, true).toBool());

    setupView();
    setupDelegate();
    setupColumns();
    setupLayout();
    connectSignals();

    m_view->expandAll();
    handleModelChanged();
}

void ChangedDocumentsWidget::setupView()
{
    m_view = new TreeView(this);
    setFocusProxy(m_view);
    m_view->setModel(m_proxy);
    m_view->setIndentation(kViewIndentation);
    m_view->setExpandsOnDoubleClick(false);
    m_view->setHeaderHidden(true);
    m_view->setTextElideMode(Qt::ElideMiddle);
    m_view->setFrameStyle(QFrame::NoFrame);
    m_view->setAttribute(Qt::WA_MacShowFocusRect, false);
    m_view->setActivationMode(SingleClickActivation);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
}

void ChangedDocumentsWidget::setupDelegate()
{
    auto delegate = new ChangedDocumentsDelegate(m_view);
    m_view->setItemDelegate(delegate);

    m_view->viewport()->setAttribute(Qt::WA_Hover);
    m_view->viewport()->setMouseTracking(true);
    connect(m_view, &QAbstractItemView::entered, this, [this, delegate](const QModelIndex &index) {
        delegate->setHoveredIndex(index);
        m_view->viewport()->update();
    });
    connect(m_view, &QAbstractItemView::viewportEntered, this, [this, delegate] {
        delegate->setHoveredIndex({});
        m_view->viewport()->update();
    });
}

void ChangedDocumentsWidget::setupColumns()
{
    QHeaderView *header = m_view->header();
    header->setStretchLastSection(false);
    header->setMinimumSectionSize(0);
    header->setSectionResizeMode(ChangedDocumentsModel::FileNameColumn, QHeaderView::Stretch);

    const int actionWidth = m_view->fontMetrics().height() + kActionIconMargin;
    for (int column : {int(ChangedDocumentsModel::RevertColumn),
                       int(ChangedDocumentsModel::DiffColumn),
                       int(ChangedDocumentsModel::StageColumn)}) {
        header->setSectionResizeMode(column, QHeaderView::Fixed);
        header->resizeSection(column, actionWidth);
    }
    header->resizeSection(ChangedDocumentsModel::StageColumn, actionWidth + kTrailingPadding);
}

void ChangedDocumentsWidget::setupLayout()
{
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
}

void ChangedDocumentsWidget::connectSignals()
{
    connect(m_view, &QAbstractItemView::activated,
            this, &ChangedDocumentsWidget::handleActivated);
    connect(
        m_view,
        &QWidget::customContextMenuRequested,
        this,
        &ChangedDocumentsWidget::contextMenuRequested);
    connect(
        EditorManager::instance(),
        &EditorManager::currentEditorChanged,
        this,
        &ChangedDocumentsWidget::updateCurrentItem);

    const auto onRowsChanged = [this](const QModelIndex &parent) {
        if (!parent.isValid())
            m_view->expandAll();
        handleModelChanged();
    };
    connect(m_proxy, &QAbstractItemModel::rowsInserted, this, onRowsChanged);
    connect(m_proxy, &QAbstractItemModel::rowsRemoved, this, onRowsChanged);
    connect(m_proxy, &QAbstractItemModel::modelReset, this, [this] {
        m_view->expandAll();
        handleModelChanged();
    });
}

void ChangedDocumentsWidget::handleModelChanged()
{
    m_emptyLabel->setVisible(m_proxy->rowCount() == 0);
    updateCurrentItem(EditorManager::currentEditor());
}

QModelIndex ChangedDocumentsWidget::indexOfFile(const FilePath &filePath) const
{
    for (int groupRow = 0; groupRow < m_proxy->rowCount(); ++groupRow) {
        const QModelIndex group = m_proxy->index(groupRow, 0);
        for (int row = 0; row < m_proxy->rowCount(group); ++row) {
            const QModelIndex index = m_proxy->index(row, 0, group);
            if (FilePath::fromVariant(index.data(FilePathRole)) == filePath)
                return index;
        }
    }
    return {};
}

void ChangedDocumentsWidget::updateCurrentItem(IEditor *editor)
{
    const FilePath filePath = editor && editor->document() ? editor->document()->filePath()
                                                           : FilePath();
    const QModelIndex index = filePath.isEmpty() ? QModelIndex() : indexOfFile(filePath);
    if (!index.isValid()) {
        m_view->selectionModel()->clearSelection();
        m_view->selectionModel()->clearCurrentIndex();
        return;
    }
    if (index == m_view->currentIndex())
        return;
    m_view->setCurrentIndex(index);
    m_view->selectionModel()->select(
        index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    m_view->scrollTo(index);
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

void ChangedDocumentsWidget::handleActivated(const QModelIndex &index)
{
    const auto filePath = FilePath::fromVariant(index.data(FilePathRole));
    if (filePath.isEmpty()) {
        const QModelIndex group = index.siblingAtColumn(0);
        m_view->setExpanded(group, !m_view->isExpanded(group));
        return;
    }
    const auto state = VcsFileState(index.data(ChangedDocumentsModel::StateRole).toInt());

    switch (index.column()) {
    case ChangedDocumentsModel::DiffColumn: {
        FilePath topLevel;
        if (IVersionControl *vc = VcsManager::findVersionControlForDirectory(filePath, &topLevel))
            vc->vcsDiff(topLevel, filePath.relativeChildPath(topLevel));
        return;
    }
    case ChangedDocumentsModel::RevertColumn:
        revertFile(filePath, state);
        return;
    case ChangedDocumentsModel::StageColumn:
        applyStageAction(index, filePath);
        return;
    default:
        if (state == VcsFileState::Deleted)
            return;
        EditorManager::openEditor(filePath);
    }
}

void ChangedDocumentsWidget::applyStageAction(const QModelIndex &index, const FilePath &filePath)
{
    const auto state = VcsFileState(index.data(ChangedDocumentsModel::StateRole).toInt());
    const bool staged = index.data(ChangedDocumentsModel::StagedRole).toBool();
    const StageAction action = stageActionFor(state, staged);
    if (action == StageAction::None)
        return;

    const FilePath repository = gitRepositoryFor(filePath);
    if (repository.isEmpty())
        return;
    const QString relativePath = filePath.relativeChildPath(repository).path();

    if (action == StageAction::Unstage)
        unstageFile(repository, relativePath, state);
    else
        stageFile(repository, relativePath);
    m_tracker->requestRefresh(repository);
}

static bool confirmRevert(const FilePath &filePath)
{
    const auto answer = QMessageBox::question(
        ICore::dialogParent(),
        Tr::tr("Confirm File Changes"),
        Tr::tr("<p>Undo <b>all</b> changes to the file \"%1\"?</p>"
               "<p>Note: These changes will be lost.</p>").arg(filePath.fileName()),
        QMessageBox::Yes | QMessageBox::No);
    return answer == QMessageBox::Yes;
}

void ChangedDocumentsWidget::revertFile(const FilePath &filePath, VcsFileState state)
{
    if (!isRevertable(state))
        return;
    const FilePath repository = gitRepositoryFor(filePath);
    if (repository.isEmpty())
        return;
    if (state == VcsFileState::Modified && !confirmRevert(filePath))
        return;

    const QString relativePath = filePath.relativeChildPath(repository).path();
    QString errorMessage;
    if (!checkoutFile(repository, relativePath, &errorMessage)) {
        QMessageBox::warning(
            ICore::dialogParent(),
            Tr::tr("Revert Failed"),
            errorMessage.isEmpty() ? Tr::tr("Could not revert \"%1\".").arg(filePath.fileName())
                                   : errorMessage);
        return;
    }
    m_tracker->requestRefresh(repository);
}

void ChangedDocumentsWidget::contextMenuRequested(const QPoint &pos)
{
    const QModelIndex index = m_view->indexAt(pos);
    if (!index.isValid())
        return;
    const auto filePath = FilePath::fromVariant(index.data(FilePathRole));
    if (filePath.isEmpty())
        return;
    const auto state = VcsFileState(index.data(ChangedDocumentsModel::StateRole).toInt());

    QMenu menu;
    QAction *openAction = menu.addAction(Tr::tr("Open"), this, [filePath] {
        EditorManager::openEditor(filePath);
    });
    openAction->setEnabled(state != VcsFileState::Deleted);
    menu.addAction(Tr::tr("Open Containing Folder"), this, [filePath] {
        Core::FileUtils::showInGraphicalShell(filePath);
    });
    menu.addAction(Tr::tr("Copy Full Path"), this, [filePath] {
        setClipboardAndSelection(filePath.toUserOutput());
    });
    menu.exec(m_view->mapToGlobal(pos));
}

} // namespace ChangesPanel

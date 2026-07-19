// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "changespanelwidget.h"

#include "changeddocumentsmodel.h"
#include "changespanelsettings.h"
#include "changespaneltr.h"
#include "gitfileactions.h"
#include "gitstatustracker.h"
#include "repositorysectionwidget.h"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/icore.h>
#include <coreplugin/idocument.h>

#include <utils/icon.h>
#include <utils/stylehelper.h>
#include <utils/theme/theme.h>

#include <QAction>
#include <QLabel>
#include <QMenu>
#include <QScrollArea>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Core;
using namespace Utils;

namespace ChangesPanel {

ChangesPanelWidget::ChangesPanelWidget(
    ChangedDocumentsModel *model, GitStatusTracker *tracker, GitFileActions *actions)
    : m_model(model)
    , m_tracker(tracker)
    , m_actions(actions)
{
    setWindowTitle(Tr::tr("Changes"));

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

    m_content = new QWidget;
    m_contentLayout = new QVBoxLayout(m_content);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(0);
    m_contentLayout->setSizeConstraint(QLayout::SetMinimumSize);
    m_contentLayout->addStretch();

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setWidget(m_content);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_emptyLabel);
    layout->addWidget(m_scrollArea);

    connect(m_tracker, &GitStatusTracker::repositoriesChanged,
            this, &ChangesPanelWidget::reconcileSections);
    connect(EditorManager::instance(), &EditorManager::currentEditorChanged,
            this, &ChangesPanelWidget::updateCurrentItem);

    reconcileSections();
}

void ChangesPanelWidget::reconcileSections()
{
    for (RepositorySectionWidget *section : std::as_const(m_sections)) {
        section->hide();
        section->deleteLater();
    }
    m_sections.clear();
    while (QLayoutItem *item = m_contentLayout->takeAt(0))
        delete item;

    const QList<FilePath> wanted = m_tracker->repositories();
    const bool singleRepo = wanted.size() == 1;

    for (const FilePath &repository : wanted) {
        auto section
            = new RepositorySectionWidget(repository, m_model, m_tracker, m_actions, m_content);
        m_sections.insert(repository, section);
        connect(section, &RepositorySectionWidget::visibleRowsChanged,
                this, &ChangesPanelWidget::scheduleSelectionSync);
        m_contentLayout->addWidget(section, singleRepo ? 1 : 0);
    }
    if (!singleRepo)
        m_contentLayout->addStretch();

    for (RepositorySectionWidget *section : std::as_const(m_sections))
        section->setChrome(!singleRepo);

    updateEmptyState();
    updateCurrentItem(EditorManager::currentEditor());
}

void ChangesPanelWidget::scheduleSelectionSync()
{
    if (m_selectionSyncPending)
        return;
    m_selectionSyncPending = true;
    QTimer::singleShot(0, this, [this] {
        m_selectionSyncPending = false;
        updateCurrentItem(EditorManager::currentEditor());
    });
}

void ChangesPanelWidget::updateEmptyState()
{
    const bool empty = m_sections.isEmpty();
    m_emptyLabel->setVisible(empty);
    m_scrollArea->setVisible(!empty);
}

void ChangesPanelWidget::updateCurrentItem(IEditor *editor)
{
    for (RepositorySectionWidget *section : std::as_const(m_sections)) {
        if (section->viewHasFocus())
            return;
    }

    const FilePath filePath = editor && editor->document() ? editor->document()->filePath()
                                                           : FilePath();

    RepositorySectionWidget *target = nullptr;
    if (!filePath.isEmpty()) {
        int bestDepth = -1;
        for (RepositorySectionWidget *section : std::as_const(m_sections)) {
            const FilePath repository = section->repository();
            if (filePath == repository || filePath.isChildOf(repository)) {
                const int depth = repository.pathView().size();
                if (depth > bestDepth) {
                    bestDepth = depth;
                    target = section;
                }
            }
        }
    }

    for (RepositorySectionWidget *section : std::as_const(m_sections)) {
        if (section == target)
            section->selectFile(filePath);
        else
            section->clearSelection();
    }
}

QToolButton *ChangesPanelWidget::createMenuButton()
{
    auto button = new QToolButton;
    static const QIcon menuIcon
        = Icon({{":/changespanel/icons/morevert.png", Theme::IconsBaseColor}}, Icon::Tint).icon();
    button->setIcon(menuIcon);
    button->setToolTip(Tr::tr("Git Menu"));
    button->setPopupMode(QToolButton::InstantPopup);
    button->setProperty(StyleHelper::C_NO_ARROW, true);

    auto menu = new QMenu(button);
    if (ActionContainer *gitContainer = ActionManager::actionContainer(Utils::Id("Git"))) {
        if (QMenu *gitMenu = gitContainer->menu())
            menu->addActions(gitMenu->actions());
    }

    button->setMenu(menu);
    return button;
}

QToolButton *ChangesPanelWidget::createGitClientButton()
{
    auto button = new QToolButton;
    static const QIcon clientIcon
        = Icon({{":/changespanel/icons/gitclient.png", Theme::IconsBaseColor}}, Icon::Tint).icon();
    button->setIcon(clientIcon);

    const auto updateEnabled = [this, button] {
        const bool configured
            = !settings().gitClientRepositoryCommand().trimmed().isEmpty();
        const bool hasRepository = !m_tracker->repositories().isEmpty();
        button->setEnabled(configured && hasRepository);
        if (!configured) {
            button->setToolTip(
                Tr::tr("No Git client command is configured. Set it in "
                       "Preferences > Version Control > Changes Panel."));
        } else if (!hasRepository) {
            button->setToolTip(Tr::tr("No repository is open."));
        } else {
            button->setToolTip(Tr::tr("Open in Git Client"));
        }
    };
    updateEnabled();
    connect(&settings().gitClientRepositoryCommand, &Utils::BaseAspect::changed,
            button, updateEnabled);
    connect(m_tracker, &GitStatusTracker::repositoriesChanged, button, updateEnabled);

    connect(button, &QToolButton::clicked, this, [this, button] {
        const QList<FilePath> repositories = m_tracker->repositories();
        if (repositories.isEmpty())
            return;
        if (repositories.size() == 1) {
            m_actions->openRepositoryInGitClient(repositories.first());
            return;
        }
        QMenu menu;
        menu.setToolTipsVisible(true);
        for (const FilePath &repository : repositories) {
            const RepositoryInfo info = m_tracker->repositoryInfo(repository);
            QString text = repository.fileName();
            if (info.isSubmodule && !info.parentRepository.isEmpty()) {
                const QString path
                    = repository.relativeChildPath(info.parentRepository).parentDir().path();
                if (!path.isEmpty())
                    text = QString("%1 — %2").arg(text, path);
            }
            QAction *action = menu.addAction(text, this, [this, repository] {
                m_actions->openRepositoryInGitClient(repository);
            });
            action->setToolTip(repository.toUserOutput());
        }
        menu.exec(button->mapToGlobal(QPoint(0, button->height())));
    });
    return button;
}

} // namespace ChangesPanel

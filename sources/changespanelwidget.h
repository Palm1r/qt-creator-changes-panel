// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <utils/filepath.h>

#include <QHash>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QLabel;
class QScrollArea;
class QToolButton;
class QVBoxLayout;
QT_END_NAMESPACE

namespace Core {
class IEditor;
}

namespace ChangesPanel {

class ChangedDocumentsModel;
class GitFileActions;
class GitStatusTracker;
class RepositorySectionWidget;

class ChangesPanelWidget final : public QWidget
{
    Q_OBJECT

public:
    ChangesPanelWidget(
        ChangedDocumentsModel *model, GitStatusTracker *tracker, GitFileActions *actions);

    QToolButton *createMenuButton();
    QToolButton *createGitClientButton();

private:
    void reconcileSections();
    void updateEmptyState();
    void updateCurrentItem(Core::IEditor *editor);
    void scheduleSelectionSync();

    ChangedDocumentsModel *m_model = nullptr;
    GitStatusTracker *m_tracker = nullptr;
    GitFileActions *m_actions = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_content = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QHash<Utils::FilePath, RepositorySectionWidget *> m_sections;
    bool m_selectionSyncPending = false;
};

} // namespace ChangesPanel

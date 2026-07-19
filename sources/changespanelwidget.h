// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <utils/filepath.h>

#include <QHash>
#include <QWidget>

#include <functional>

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
    QToolButton *createDiffButton();

private:
    void reconcileSections();
    void updateEmptyState();
    void updateCurrentItem(Core::IEditor *editor);
    void scheduleSelectionSync();
    void runForRepository(
        QToolButton *button,
        const std::function<void(const Utils::FilePath &)> &action,
        const std::function<bool(const Utils::FilePath &)> &accept = {});
    void diffAllChanges(const Utils::FilePath &repository) const;
    bool repositoryHasChanges(const Utils::FilePath &repository) const;

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

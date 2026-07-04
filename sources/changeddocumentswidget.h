// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <coreplugin/vcsfilestate.h>

#include <QWidget>

QT_BEGIN_NAMESPACE
class QLabel;
class QToolButton;
QT_END_NAMESPACE

namespace Core {
class IEditor;
}

namespace Utils {
class FilePath;
class TreeView;
}

namespace ChangesPanel {

class ChangedDocumentsModel;
class ChangedDocumentsProxyModel;
class GitStatusTracker;

class ChangedDocumentsWidget final : public QWidget
{
    Q_OBJECT

public:
    ChangedDocumentsWidget(ChangedDocumentsModel *model, GitStatusTracker *tracker);

    QToolButton *createMenuButton();

private:
    void setupView();
    void setupDelegate();
    void setupColumns();
    void setupLayout();
    void connectSignals();

    void scheduleModelChangedUpdate();
    void handleModelChanged();
    void updateCurrentItem(Core::IEditor *editor);
    QModelIndex indexOfFile(const Utils::FilePath &filePath) const;

    void handleActivated(const QModelIndex &index);
    void contextMenuRequested(const QPoint &pos);
    void applyStageAction(const QModelIndex &index, const Utils::FilePath &filePath);
    void revertFile(const Utils::FilePath &filePath, Core::VcsFileState state);

    GitStatusTracker *m_tracker = nullptr;
    Utils::TreeView *m_view = nullptr;
    ChangedDocumentsProxyModel *m_proxy = nullptr;
    QLabel *m_emptyLabel = nullptr;
    bool m_modelChangedPending = false;
};

} // namespace ChangesPanel

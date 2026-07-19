// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include "changeddocumentsmodel.h"

#include <utils/filepath.h>

#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QWidget>

#include <optional>

QT_BEGIN_NAMESPACE
class QEvent;
class QFrame;
class QLabel;
QT_END_NAMESPACE

namespace Utils {
class TreeView;
}

namespace ChangesPanel {

enum class FileEntryAction;
enum class StageAction;

class ChangedDocumentsProxyModel;
class ChangedDocumentsDelegate;
class GitFileActions;
class GitStatusTracker;

class RepositorySectionWidget final : public QWidget
{
    Q_OBJECT

public:
    RepositorySectionWidget(
        const Utils::FilePath &repository,
        ChangedDocumentsModel *model,
        GitStatusTracker *tracker,
        GitFileActions *actions,
        QWidget *parent = nullptr);

    Utils::FilePath repository() const { return m_repository; }

    void setChrome(bool show);
    bool selectFile(const Utils::FilePath &filePath);
    void clearSelection();
    bool viewHasFocus() const;

signals:
    void visibleRowsChanged();

private:
    void setupHeader();
    void setupView();
    void setupDelegate();
    void setupColumns();
    void connectSignals();

    void updateRepositoryInfo();
    void scheduleHeightUpdate();
    void updateViewHeight();
    void setCollapsed(bool collapsed, bool userInitiated);
    void updateChevron();
    void updateAutoCollapse();
    bool hasVisibleChanges() const;
    bool eventFilter(QObject *watched, QEvent *event) override;

    void handleActivated(const QModelIndex &index);
    void triggerFileAction(FileEntryAction action, const QModelIndex &index);
    void contextMenuRequested(const QPoint &pos);
    void handleStageClicked(const QModelIndex &index);
    void handleGroupStageClicked(const QModelIndex &group, StageAction action);
    void handleRevertClicked(const QModelIndex &index);
    void handleGroupRevertClicked(const QModelIndex &group);

    struct ZoneHit
    {
        QPersistentModelIndex index;
        ChangedDocumentsModel::Column column = ChangedDocumentsModel::ColumnCount;

        friend bool operator==(const ZoneHit &a, const ZoneHit &b)
        {
            return a.index == b.index && a.column == b.column;
        }
    };
    std::optional<ZoneHit> actionZoneAt(const QPoint &pos) const;
    void triggerZoneAction(const QModelIndex &index, ChangedDocumentsModel::Column column);
    bool handleViewportEvent(QEvent *event);
    void updateHoverZone(const std::optional<ZoneHit> &hit);

    Utils::FilePath m_repository;
    ChangedDocumentsModel *m_model = nullptr;
    GitStatusTracker *m_tracker = nullptr;
    GitFileActions *m_actions = nullptr;
    ChangedDocumentsProxyModel *m_proxy = nullptr;
    Utils::TreeView *m_view = nullptr;
    ChangedDocumentsDelegate *m_delegate = nullptr;

    QFrame *m_separator = nullptr;
    QWidget *m_header = nullptr;
    QLabel *m_chevronLabel = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_pathLabel = nullptr;
    QLabel *m_submoduleLabel = nullptr;

    std::optional<ZoneHit> m_pressedZone = std::nullopt;
    std::optional<ZoneHit> m_hoverZone = std::nullopt;

    bool m_collapsed = false;
    bool m_userToggled = false;
    bool m_chromeless = false;
    bool m_heightUpdatePending = false;
};

} // namespace ChangesPanel

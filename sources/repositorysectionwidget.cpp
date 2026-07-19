// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "repositorysectionwidget.h"

#include "changeddocumentsdelegate.h"
#include "changeddocumentsmodel.h"
#include "changeddocumentsproxymodel.h"
#include "changespanelconstants.h"
#include "changespanelsettings.h"
#include "changespaneltr.h"
#include "gitfileactions.h"
#include "gitstatustracker.h"

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/fileutils.h>
#include <coreplugin/icore.h>

#include <utils/aspects.h>
#include <utils/filepath.h>
#include <utils/itemviews.h>
#include <utils/stringutils.h>
#include <utils/theme/theme.h>

#include <QAction>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPointer>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

using namespace Core;
using namespace Utils;

namespace ChangesPanel {

constexpr int kViewIndentation = 12;
constexpr int kViewHeightPadding = 4;
constexpr qreal kHeaderTextTint = 0.08;
constexpr qreal kChevronFontScale = 1.3;

static void reportIfFailed(const Result<> &result, const QString &title)
{
    if (!result)
        QMessageBox::warning(ICore::dialogParent(), title, result.error());
}

RepositorySectionWidget::RepositorySectionWidget(
    const FilePath &repository,
    ChangedDocumentsModel *model,
    GitStatusTracker *tracker,
    GitFileActions *actions,
    QWidget *parent)
    : QWidget(parent)
    , m_repository(repository)
    , m_model(model)
    , m_tracker(tracker)
    , m_actions(actions)
{
    m_proxy = new ChangedDocumentsProxyModel(this);
    m_proxy->setSourceModel(model);
    m_proxy->setRepositoryFilter(repository);

    setupHeader();
    setupView();
    setupDelegate();
    setupColumns();

    m_separator = new QFrame(this);
    m_separator->setFrameShape(QFrame::HLine);
    m_separator->setFrameShadow(QFrame::Plain);
    m_separator->setFixedHeight(1);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_separator);
    layout->addWidget(m_header);
    layout->addWidget(m_view);

    connectSignals();

    updateRepositoryInfo();
    m_view->expandAll();
    updateChevron();
    updateAutoCollapse();
    scheduleHeightUpdate();
}

void RepositorySectionWidget::setupHeader()
{
    m_header = new QWidget(this);
    m_header->setCursor(Qt::PointingHandCursor);
    m_header->setAutoFillBackground(true);
    QPalette headerPalette = m_header->palette();
    const QColor base = headerPalette.color(QPalette::Window);
    const QColor text = headerPalette.color(QPalette::WindowText);
    headerPalette.setColor(
        QPalette::Window,
        QColor::fromRgbF(
            base.redF() * (1 - kHeaderTextTint) + text.redF() * kHeaderTextTint,
            base.greenF() * (1 - kHeaderTextTint) + text.greenF() * kHeaderTextTint,
            base.blueF() * (1 - kHeaderTextTint) + text.blueF() * kHeaderTextTint));
    m_header->setPalette(headerPalette);
    m_header->installEventFilter(this);

    m_chevronLabel = new QLabel(m_header);
    m_chevronLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    QFont chevronFont = m_chevronLabel->font();
    chevronFont.setPointSizeF(chevronFont.pointSizeF() * kChevronFontScale);
    m_chevronLabel->setFont(chevronFont);
    QPalette chevronPalette = m_chevronLabel->palette();
    chevronPalette.setColor(QPalette::WindowText, creatorColor(Theme::TextColorDisabled));
    m_chevronLabel->setPalette(chevronPalette);

    m_submoduleLabel = new QLabel(Tr::tr("submodule"), m_header);
    m_submoduleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    QPalette submodulePalette = m_submoduleLabel->palette();
    submodulePalette.setColor(QPalette::WindowText, creatorColor(Theme::TextColorDisabled));
    m_submoduleLabel->setPalette(submodulePalette);
    m_submoduleLabel->setVisible(false);

    m_nameLabel = new QLabel(m_repository.fileName(), m_header);
    m_nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    QFont nameFont = m_nameLabel->font();
    nameFont.setBold(true);
    m_nameLabel->setFont(nameFont);

    m_pathLabel = new QLabel(m_header);
    m_pathLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    QPalette pathPalette = m_pathLabel->palette();
    pathPalette.setColor(QPalette::WindowText, creatorColor(Theme::TextColorDisabled));
    m_pathLabel->setPalette(pathPalette);
    m_pathLabel->setVisible(false);

    auto headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(6, 3, 6, 3);
    headerLayout->setSpacing(6);
    headerLayout->addWidget(m_chevronLabel);
    headerLayout->addWidget(m_nameLabel);
    headerLayout->addWidget(m_pathLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_submoduleLabel);
}

void RepositorySectionWidget::setupView()
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
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void RepositorySectionWidget::setupDelegate()
{
    m_delegate = new ChangedDocumentsDelegate(m_view);
    m_view->setItemDelegate(m_delegate);

    m_view->viewport()->setAttribute(Qt::WA_Hover);
    m_view->viewport()->setMouseTracking(true);
    m_view->viewport()->installEventFilter(this);
}

void RepositorySectionWidget::setupColumns()
{
    QHeaderView *header = m_view->header();
    header->setStretchLastSection(false);
    header->setMinimumSectionSize(0);
    header->setSectionResizeMode(ChangedDocumentsModel::FileNameColumn, QHeaderView::Stretch);

    for (int column : {int(ChangedDocumentsModel::GitClientColumn),
                       int(ChangedDocumentsModel::RevertColumn),
                       int(ChangedDocumentsModel::DiffColumn),
                       int(ChangedDocumentsModel::StageColumn)}) {
        m_view->setColumnHidden(column, true);
    }

    const std::initializer_list<Utils::BaseAspect *> repaintAspects
        = {&settings().showDiffButton,
           &settings().showStageButton,
           &settings().showRevertButton,
           &settings().showGitClientButton,
           &settings().fileClickAction,
           &settings().gitClientFileCommand};
    for (Utils::BaseAspect *aspect : repaintAspects) {
        connect(aspect, &Utils::BaseAspect::changed, this,
                [this] { m_view->viewport()->update(); });
    }
}

void RepositorySectionWidget::connectSignals()
{
    connect(m_view, &QAbstractItemView::activated,
            this, &RepositorySectionWidget::handleActivated);
    connect(m_view, &QWidget::customContextMenuRequested,
            this, &RepositorySectionWidget::contextMenuRequested);

    connect(m_proxy, &QAbstractItemModel::rowsInserted, this,
            [this](const QModelIndex &parent, int first, int last) {
                if (!parent.isValid()) {
                    for (int row = first; row <= last; ++row)
                        m_view->expand(m_proxy->index(row, 0));
                }
                updateAutoCollapse();
                scheduleHeightUpdate();
                emit visibleRowsChanged();
            });
    connect(m_proxy, &QAbstractItemModel::rowsRemoved, this, [this] {
        updateAutoCollapse();
        scheduleHeightUpdate();
        emit visibleRowsChanged();
    });
    connect(m_proxy, &QAbstractItemModel::modelReset, this, [this] {
        m_view->expandAll();
        updateAutoCollapse();
        scheduleHeightUpdate();
        emit visibleRowsChanged();
    });
    connect(m_view, &QTreeView::expanded, this, [this] { scheduleHeightUpdate(); });
    connect(m_view, &QTreeView::collapsed, this, [this] { scheduleHeightUpdate(); });

    connect(m_tracker, &GitStatusTracker::repositoryInfoChanged, this,
            [this](const FilePath &repository) {
                if (repository != m_repository)
                    return;
                updateRepositoryInfo();
                updateAutoCollapse();
            });
}

void RepositorySectionWidget::updateRepositoryInfo()
{
    const RepositoryInfo info = m_tracker->repositoryInfo(m_repository);

    QString path;
    if (info.isSubmodule && !info.parentRepository.isEmpty())
        path = m_repository.relativeChildPath(info.parentRepository).parentDir().path();
    m_pathLabel->setText(path);
    m_pathLabel->setVisible(!path.isEmpty());
    m_submoduleLabel->setVisible(info.isSubmodule);
}

void RepositorySectionWidget::setCollapsed(bool collapsed, bool userInitiated)
{
    if (userInitiated)
        m_userToggled = true;
    if (m_collapsed != collapsed) {
        m_collapsed = collapsed;
        m_view->setVisible(!collapsed);
    }
    updateChevron();
}

void RepositorySectionWidget::updateChevron()
{
    m_chevronLabel->setText(QString(QChar(m_collapsed ? 0x25B8 : 0x25BE)));
}

bool RepositorySectionWidget::hasVisibleChanges() const
{
    return m_proxy->rowCount() > 0;
}

void RepositorySectionWidget::updateAutoCollapse()
{
    if (m_chromeless) {
        setCollapsed(false, false);
        return;
    }
    if (m_userToggled)
        return;
    setCollapsed(!hasVisibleChanges(), false);
}

void RepositorySectionWidget::setChrome(bool show)
{
    m_chromeless = !show;
    m_header->setVisible(show);
    m_separator->setVisible(show);
    if (m_chromeless) {
        m_userToggled = false;
        m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_view->setMinimumHeight(0);
        m_view->setMaximumHeight(QWIDGETSIZE_MAX);
        m_view->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        setCollapsed(false, false);
    } else {
        m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_view->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        updateAutoCollapse();
        scheduleHeightUpdate();
    }
}

bool RepositorySectionWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_header && event->type() == QEvent::MouseButtonRelease && !m_chromeless) {
        setCollapsed(!m_collapsed, true);
        return true;
    }
    if (m_view && watched == m_view->viewport())
        return handleViewportEvent(event);
    return QWidget::eventFilter(watched, event);
}

std::optional<RepositorySectionWidget::ZoneHit> RepositorySectionWidget::actionZoneAt(
    const QPoint &pos) const
{
    const QModelIndex index = m_view->indexAt(pos);
    if (!index.isValid())
        return std::nullopt;
    const QRect rowRect = m_view->visualRect(index);
    const RowActionZones row = rowActionZones(rowRect, m_view->fontMetrics(), index);
    if (const RowActionZone *zone = row.zoneAt(pos))
        return ZoneHit{index, zone->column};
    return std::nullopt;
}

void RepositorySectionWidget::triggerZoneAction(const QModelIndex &index,
                                                ChangedDocumentsModel::Column column)
{
    if (isGroupHeader(index)) {
        if (column == ChangedDocumentsModel::RevertColumn) {
            handleGroupRevertClicked(index.siblingAtColumn(0));
            return;
        }
        const StageAction action = groupStageActionAt(index);
        if (action != StageAction::None)
            handleGroupStageClicked(index.siblingAtColumn(0), action);
        return;
    }
    switch (column) {
    case ChangedDocumentsModel::DiffColumn:
        triggerFileAction(diffColumnActionFor(fileStateAt(index)), index);
        return;
    case ChangedDocumentsModel::RevertColumn:
        handleRevertClicked(index);
        return;
    case ChangedDocumentsModel::StageColumn:
        handleStageClicked(index);
        return;
    case ChangedDocumentsModel::GitClientColumn: {
        const FilePath repository = repositoryAt(index);
        if (repository.isEmpty())
            return;
        m_actions->openFileInGitClient(repository, relativePathAt(index));
        return;
    }
    case ChangedDocumentsModel::FileNameColumn:
    case ChangedDocumentsModel::ColumnCount:
        return;
    }
}

bool RepositorySectionWidget::handleViewportEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseMove: {
        const QPoint pos = static_cast<QMouseEvent *>(event)->position().toPoint();
        m_delegate->setHoverPosition(pos);
        updateHoverZone(actionZoneAt(pos));
        return false;
    }
    case QEvent::Leave:
        m_delegate->clearHoverPosition();
        m_pressedZone.reset();
        updateHoverZone(std::nullopt);
        return false;
    case QEvent::MouseButtonPress: {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() != Qt::LeftButton)
            return false;
        m_pressedZone = actionZoneAt(mouse->position().toPoint());
        return m_pressedZone.has_value();
    }
    case QEvent::MouseButtonDblClick: {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() != Qt::LeftButton)
            return false;
        m_pressedZone.reset();
        return actionZoneAt(mouse->position().toPoint()).has_value();
    }
    case QEvent::MouseButtonRelease: {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() != Qt::LeftButton)
            return false;
        const std::optional<ZoneHit> pressed = std::exchange(m_pressedZone, std::nullopt);
        if (!pressed)
            return false;
        const std::optional<ZoneHit> hit = actionZoneAt(mouse->position().toPoint());
        if (hit && *hit == *pressed) {
            const QPointer<RepositorySectionWidget> guard(this);
            triggerZoneAction(hit->index, hit->column);
            if (!guard)
                return true;
        }
        return true;
    }
    default:
        return false;
    }
}

void RepositorySectionWidget::updateHoverZone(const std::optional<ZoneHit> &hit)
{
    if (m_hoverZone == hit)
        return;
    if (m_hoverZone && m_hoverZone->index.isValid())
        m_view->viewport()->update(m_view->visualRect(m_hoverZone->index));
    m_hoverZone = hit;
    if (m_hoverZone)
        m_view->viewport()->update(m_view->visualRect(m_hoverZone->index));
}

void RepositorySectionWidget::scheduleHeightUpdate()
{
    if (m_heightUpdatePending)
        return;
    m_heightUpdatePending = true;
    QTimer::singleShot(0, this, [this] {
        m_heightUpdatePending = false;
        updateViewHeight();
    });
}

void RepositorySectionWidget::updateViewHeight()
{
    if (m_chromeless)
        return;
    int height = 2 * m_view->frameWidth();
    const int groupCount = m_proxy->rowCount();
    for (int group = 0; group < groupCount; ++group) {
        const QModelIndex groupIndex = m_proxy->index(group, 0);
        height += m_view->sizeHintForIndex(groupIndex).height();
        if (!m_view->isExpanded(groupIndex))
            continue;
        const int childCount = m_proxy->rowCount(groupIndex);
        for (int child = 0; child < childCount; ++child)
            height += m_view->sizeHintForIndex(m_proxy->index(child, 0, groupIndex)).height();
    }
    m_view->setFixedHeight((std::max)(height + kViewHeightPadding, 1));
}

bool RepositorySectionWidget::selectFile(const FilePath &filePath)
{
    const QModelIndex index = m_proxy->mapFromSource(m_model->indexForFile(filePath));
    if (!index.isValid())
        return false;
    if (index != m_view->currentIndex()) {
        m_view->setCurrentIndex(index);
        m_view->selectionModel()->select(
            index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        m_view->scrollTo(index);
    }
    return true;
}

void RepositorySectionWidget::clearSelection()
{
    m_view->selectionModel()->clearSelection();
    m_view->selectionModel()->clearCurrentIndex();
}

bool RepositorySectionWidget::viewHasFocus() const
{
    return m_view->hasFocus();
}

void RepositorySectionWidget::handleActivated(const QModelIndex &index)
{
    if (isGroupHeader(index)) {
        const QModelIndex group = index.siblingAtColumn(0);
        m_view->setExpanded(group, !m_view->isExpanded(group));
        return;
    }
    triggerFileAction(rowClickActionFor(fileStateAt(index)), index);
}

void RepositorySectionWidget::triggerFileAction(FileEntryAction action, const QModelIndex &index)
{
    switch (action) {
    case FileEntryAction::OpenEditor:
        EditorManager::openEditor(filePathAt(index));
        return;
    case FileEntryAction::ShowDiff:
        m_actions->diffFile(repositoryAt(index), relativePathAt(index), stagedAt(index));
        return;
    case FileEntryAction::None:
        return;
    }
}

void RepositorySectionWidget::handleStageClicked(const QModelIndex &index)
{
    const StageAction action = stageActionFor(fileStateAt(index), stagedAt(index));
    if (action == StageAction::None)
        return;
    const StageableFile file{repositoryAt(index), relativePathAt(index), fileStateAt(index)};
    if (file.repository.isEmpty())
        return;

    reportIfFailed(m_actions->applyStageAction(action, {file}),
                   stageActionFailureTitle(action));
}

void RepositorySectionWidget::handleGroupStageClicked(const QModelIndex &group, StageAction action)
{
    QList<StageableFile> files;
    const int rows = m_proxy->rowCount(group);
    for (int row = 0; row < rows; ++row) {
        const QModelIndex child = m_proxy->index(row, 0, group);
        if (stageActionFor(fileStateAt(child), stagedAt(child)) != action)
            continue;
        const FilePath repository = repositoryAt(child);
        if (repository.isEmpty())
            continue;
        files.append({repository, relativePathAt(child), fileStateAt(child)});
    }

    reportIfFailed(m_actions->applyStageAction(action, files),
                   stageActionFailureTitle(action));
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

void RepositorySectionWidget::handleRevertClicked(const QModelIndex &index)
{
    const FileState state = fileStateAt(index);
    if (!isRevertable(state))
        return;
    const FilePath repository = repositoryAt(index);
    if (repository.isEmpty())
        return;
    const QString relativePath = relativePathAt(index);
    const QPointer<RepositorySectionWidget> guard(this);
    if (state == FileState::Modified && !confirmRevert(filePathAt(index)))
        return;
    if (!guard)
        return;

    reportIfFailed(m_actions->revertFile(repository, relativePath), Tr::tr("Revert Failed"));
}

static bool confirmGroupRevert(int fileCount)
{
    const auto answer = QMessageBox::question(
        ICore::dialogParent(),
        Tr::tr("Confirm File Changes"),
        Tr::tr("<p>Undo <b>all</b> changes to %n file(s)?</p>"
               "<p>Note: These changes will be lost.</p>", nullptr, fileCount),
        QMessageBox::Yes | QMessageBox::No);
    return answer == QMessageBox::Yes;
}

void RepositorySectionWidget::handleGroupRevertClicked(const QModelIndex &group)
{
    FilePath repository;
    QStringList relativePaths;
    const int rows = m_proxy->rowCount(group);
    for (int row = 0; row < rows; ++row) {
        const QModelIndex child = m_proxy->index(row, 0, group);
        if (!isRevertable(fileStateAt(child)))
            continue;
        const FilePath childRepository = repositoryAt(child);
        if (childRepository.isEmpty())
            continue;
        repository = childRepository;
        relativePaths.append(relativePathAt(child));
    }
    if (relativePaths.isEmpty())
        return;

    const QPointer<RepositorySectionWidget> guard(this);
    if (!confirmGroupRevert(int(relativePaths.size())))
        return;
    if (!guard)
        return;

    reportIfFailed(m_actions->revertFiles(repository, relativePaths), Tr::tr("Revert Failed"));
}

void RepositorySectionWidget::contextMenuRequested(const QPoint &pos)
{
    const QModelIndex index = m_view->indexAt(pos);
    if (!index.isValid() || isGroupHeader(index))
        return;
    const FilePath filePath = filePathAt(index);
    const FileState state = fileStateAt(index);

    QMenu menu;
    QAction *openAction = menu.addAction(Tr::tr("Open"), this, [filePath] {
        EditorManager::openEditor(filePath);
    });
    openAction->setEnabled(state != FileState::Deleted);
    menu.addAction(Tr::tr("Open Containing Folder"), this, [filePath] {
        Core::FileUtils::showInGraphicalShell(filePath);
    });
    menu.addAction(Tr::tr("Copy Full Path"), this, [filePath] {
        setClipboardAndSelection(filePath.toUserOutput());
    });
    if (settings().hasGitClientFileCommand()) {
        const FilePath repository = repositoryAt(index);
        const QString relativePath = relativePathAt(index);
        menu.addAction(Tr::tr("Open File in Git Client"), this, [this, repository, relativePath] {
            m_actions->openFileInGitClient(repository, relativePath);
        });
    } else {
        menu.addAction(Tr::tr("Open File in Git Client..."), this, [] {
            Core::ICore::showSettings(Utils::Id(Constants::SETTINGS_PAGE_ID));
        });
    }

    const StageAction stageAction = stageActionFor(state, stagedAt(index));
    const QPersistentModelIndex persistent(index);
    if (stageAction != StageAction::None || isRevertable(state))
        menu.addSeparator();
    if (stageAction != StageAction::None) {
        menu.addAction(stageAction == StageAction::Stage ? Tr::tr("Stage") : Tr::tr("Unstage"),
                       this, [this, persistent] {
                           if (persistent.isValid())
                               handleStageClicked(persistent);
                       });
    }
    if (isRevertable(state)) {
        menu.addAction(Tr::tr("Revert"), this, [this, persistent] {
            if (persistent.isValid())
                handleRevertClicked(persistent);
        });
    }
    menu.exec(m_view->mapToGlobal(pos));
}

} // namespace ChangesPanel

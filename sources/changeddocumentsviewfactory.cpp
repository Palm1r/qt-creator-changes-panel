// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "changeddocumentsviewfactory.h"

#include "changespanelconstants.h"
#include "changespanelwidget.h"
#include "changespaneltr.h"

#include <coreplugin/actionmanager/command.h>

namespace ChangesPanel {

constexpr int kNavigationPriority = 210;

ChangedDocumentsViewFactory::ChangedDocumentsViewFactory(ChangedDocumentsModel *model,
                                                         GitStatusTracker *tracker,
                                                         GitFileActions *actions)
    : m_model(model)
    , m_tracker(tracker)
    , m_actions(actions)
{
    setId(Constants::CHANGES_VIEW_ID);
    setDisplayName(Tr::tr("Changes"));
    setActivationSequence(QKeySequence(Core::useMacShortcuts ? Tr::tr("Meta+P") : Tr::tr("Alt+P")));
    setPriority(kNavigationPriority);
}

Core::NavigationView ChangedDocumentsViewFactory::createWidget()
{
    auto widget = new ChangesPanelWidget(m_model, m_tracker, m_actions);
    return {widget,
            {widget->createDiffButton(),
             widget->createGitClientButton(),
             widget->createMenuButton()}};
}

} // namespace ChangesPanel

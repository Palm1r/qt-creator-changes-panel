// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "changeddocumentsviewfactory.h"

#include "changeddocumentswidget.h"
#include "changespanelconstants.h"
#include "changespaneltr.h"

#include <coreplugin/actionmanager/command.h>

namespace ChangesPanel {

constexpr int kNavigationPriority = 210;

ChangedDocumentsViewFactory::ChangedDocumentsViewFactory(ChangedDocumentsModel *model)
    : m_model(model)
{
    setId(Constants::CHANGES_VIEW_ID);
    setDisplayName(Tr::tr("Changes"));
    setActivationSequence(QKeySequence(Core::useMacShortcuts ? Tr::tr("Meta+P") : Tr::tr("Alt+P")));
    setPriority(kNavigationPriority);
}

Core::NavigationView ChangedDocumentsViewFactory::createWidget()
{
    auto widget = new ChangedDocumentsWidget(m_model);
    return {widget, {widget->createFilterButton()}};
}

} // namespace ChangesPanel

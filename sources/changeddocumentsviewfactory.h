// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <coreplugin/inavigationwidgetfactory.h>

namespace ChangesPanel {

class ChangedDocumentsModel;
class GitFileActions;
class GitStatusTracker;

class ChangedDocumentsViewFactory final : public Core::INavigationWidgetFactory
{
public:
    ChangedDocumentsViewFactory(
        ChangedDocumentsModel *model, GitStatusTracker *tracker, GitFileActions *actions);

    Core::NavigationView createWidget() final;

private:
    ChangedDocumentsModel *m_model = nullptr;
    GitStatusTracker *m_tracker = nullptr;
    GitFileActions *m_actions = nullptr;
};

} // namespace ChangesPanel

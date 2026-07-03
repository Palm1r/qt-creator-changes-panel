// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <coreplugin/inavigationwidgetfactory.h>

namespace ChangesPanel {

class ChangedDocumentsModel;

class ChangedDocumentsViewFactory final : public Core::INavigationWidgetFactory
{
public:
    explicit ChangedDocumentsViewFactory(ChangedDocumentsModel *model);

    Core::NavigationView createWidget() final;

private:
    ChangedDocumentsModel *m_model = nullptr;
};

} // namespace ChangesPanel

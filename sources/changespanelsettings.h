// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <utils/aspects.h>

namespace ChangesPanel {

class ChangesPanelSettings final : public Utils::AspectContainer
{
public:
    ChangesPanelSettings();

    enum FileClickAction : int { OpenDiff, OpenInEditor };

    Utils::BoolAspect enabled = {this};
    Utils::BoolAspect showDiffButton = {this};
    Utils::BoolAspect showStageButton = {this};
    Utils::BoolAspect showRevertButton = {this};
    Utils::SelectionAspect fileClickAction = {this};
    Utils::StringAspect externalGitClient = {this};
};

ChangesPanelSettings &settings();

} // namespace ChangesPanel

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

    bool hasGitClientFileCommand() const;

    Utils::BoolAspect enabled = {this};
    Utils::BoolAspect showDiffButton = {this};
    Utils::BoolAspect showStageButton = {this};
    Utils::BoolAspect showRevertButton = {this};
    Utils::BoolAspect showGitClientButton = {this};
    Utils::SelectionAspect fileClickAction = {this};
    Utils::StringAspect gitClientRepositoryCommand = {this};
    Utils::StringAspect gitClientFileCommand = {this};
};

ChangesPanelSettings &settings();

} // namespace ChangesPanel

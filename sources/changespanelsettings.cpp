// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "changespanelsettings.h"

#include "changespaneltr.h"

#include <coreplugin/dialogs/ioptionspage.h>

#include <utils/hostosinfo.h>
#include <utils/layoutbuilder.h>

#include <vcsbase/vcsbaseconstants.h>

namespace ChangesPanel {

ChangesPanelSettings &settings()
{
    static ChangesPanelSettings theSettings;
    return theSettings;
}

ChangesPanelSettings::ChangesPanelSettings()
{
    setAutoApply(false);

    setSettingsGroup("ChangesPanel");

    enabled.setSettingsKey("Enabled");
    enabled.setDefaultValue(true);
    enabled.setLabelText(Tr::tr("Enable Changes panel"));

    showDiffButton.setSettingsKey("ShowDiffButton");
    showDiffButton.setDefaultValue(true);
    showDiffButton.setLabelText(Tr::tr("Diff / open"));
    showDiffButton.setToolTip(
        Tr::tr("The button complements the click action: it opens the file when "
               "clicking diffs, and diffs when clicking opens."));

    showStageButton.setSettingsKey("ShowStageButton");
    showStageButton.setDefaultValue(true);
    showStageButton.setLabelText(Tr::tr("Stage / unstage"));

    showRevertButton.setSettingsKey("ShowRevertButton");
    showRevertButton.setDefaultValue(true);
    showRevertButton.setLabelText(Tr::tr("Revert"));

    fileClickAction.setSettingsKey("FileClickAction");
    fileClickAction.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);
    fileClickAction.setUseDataAsSavedValue();
    fileClickAction.setLabelText(Tr::tr("Clicking a file:"));
    fileClickAction.setToolTip(
        Tr::tr("\"Open\" is always available in the file's context menu."));
    fileClickAction.addOption({Tr::tr("Opens the Diff"),
                               Tr::tr("Show the file's changes in the diff editor."),
                               QString("diff")});
    fileClickAction.addOption({Tr::tr("Opens the File in the Editor"),
                               Tr::tr("Open the file itself, like Open Documents does."),
                               QString("editor")});
    fileClickAction.setDefaultValue(OpenDiff);

    externalGitClient.setSettingsKey("ExternalGitClient");
    externalGitClient.setDefaultValue(
        Utils::HostOsInfo::isMacHost() ? QString("open -a \"Sublime Merge\"")
                                       : QString("smerge"));
    externalGitClient.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    externalGitClient.setLabelText(Tr::tr("External Git client command"));
    externalGitClient.setToolTip(
        Tr::tr("Run by the panel's \"Open in Git Client\" button. The repository "
               "path is appended as the last argument."));

    setLayouter([this] {
        using namespace Layouting;
        return Column {
            enabled,
            Group {
                title(Tr::tr("Show action buttons next to files")),
                Column {
                    showDiffButton,
                    showStageButton,
                    showRevertButton,
                },
            },
            Group {
                title(Tr::tr("Behavior")),
                Column {
                    fileClickAction,
                    externalGitClient,
                },
            },
            st,
        };
    });

    readSettings();
}

class ChangesPanelSettingsPage final : public Core::IOptionsPage
{
public:
    ChangesPanelSettingsPage()
    {
        setId("ChangesPanel.Settings");
        setDisplayName(Tr::tr("Changes Panel"));
        setCategory(VcsBase::Constants::VCS_SETTINGS_CATEGORY);
        setSettingsProvider([] { return &settings(); });
    }
};

const ChangesPanelSettingsPage settingsPage;

} // namespace ChangesPanel

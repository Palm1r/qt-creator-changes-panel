// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "changespanelsettings.h"

#include "changespanelconstants.h"
#include "changespaneltr.h"

#include <coreplugin/dialogs/ioptionspage.h>

#include <utils/layoutbuilder.h>

#include <vcsbase/vcsbaseconstants.h>

namespace ChangesPanel {

ChangesPanelSettings &settings()
{
    static ChangesPanelSettings theSettings;
    return theSettings;
}

bool ChangesPanelSettings::hasGitClientFileCommand() const
{
    return !gitClientFileCommand().trimmed().isEmpty();
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

    showGitClientButton.setSettingsKey("ShowGitClientButton");
    showGitClientButton.setDefaultValue(true);
    showGitClientButton.setLabelText(Tr::tr("Open in Git client"));
    showGitClientButton.setToolTip(
        Tr::tr("Shown only while an open file command is configured below."));

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

    gitClientRepositoryCommand.setSettingsKey("ExternalGitClient");
    gitClientRepositoryCommand.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    gitClientRepositoryCommand.setLabelText(Tr::tr("Open repository command:"));
    gitClientRepositoryCommand.setPlaceHolderText(QStringLiteral("smerge %{repo}"));
    gitClientRepositoryCommand.setToolTip(
        Tr::tr("Run by the panel's \"Open Repository in Git Client\" toolbar button, "
               "from the repository directory. %{repo} is replaced with the repository "
               "path; the command is run exactly as written."));

    gitClientFileCommand.setSettingsKey("ExternalGitClientFile");
    gitClientFileCommand.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    gitClientFileCommand.setLabelText(Tr::tr("Open file command:"));
    gitClientFileCommand.setPlaceHolderText(QStringLiteral("smerge log %{relativeFile}"));
    gitClientFileCommand.setToolTip(
        Tr::tr("Run by a file's \"Open File in Git Client\" context menu entry and "
               "hover button, from the repository directory. %{repo} and %{file} are "
               "replaced with absolute paths, %{relativeFile} with the file's path "
               "relative to the repository."));

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
                    showGitClientButton,
                },
            },
            Group {
                title(Tr::tr("Behavior")),
                Column {
                    fileClickAction,
                },
            },
            Group {
                title(Tr::tr("Open in Git Client")),
                Column {
                    gitClientRepositoryCommand,
                    gitClientFileCommand,
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
        setId(Constants::SETTINGS_PAGE_ID);
        setDisplayName(Tr::tr("Changes Panel"));
        setCategory(VcsBase::Constants::VCS_SETTINGS_CATEGORY);
        setSettingsProvider([] { return &settings(); });
    }
};

const ChangesPanelSettingsPage settingsPage;

} // namespace ChangesPanel

// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "gitfileactions.h"

#include <coreplugin/iversioncontrol.h>
#include <coreplugin/vcsmanager.h>

#include <git/gitclient.h>

#include <utils/filepath.h>

#include <vcsbase/vcsbaseconstants.h>

using namespace Core;
using namespace Utils;

namespace ChangesPanel {

StageAction stageActionFor(VcsFileState state, bool staged)
{
    if (staged)
        return StageAction::Unstage;
    switch (state) {
    case VcsFileState::Added:
    case VcsFileState::Modified:
    case VcsFileState::Untracked:
    case VcsFileState::Deleted:
        return StageAction::Stage;
    case VcsFileState::Unknown:
    case VcsFileState::Renamed:
    case VcsFileState::Unmerged:
        return StageAction::None;
    }
    return StageAction::None;
}

bool isRevertable(VcsFileState state)
{
    return state == VcsFileState::Modified || state == VcsFileState::Deleted;
}

FilePath gitRepositoryFor(const FilePath &filePath)
{
    FilePath topLevel;
    IVersionControl *vc = VcsManager::findVersionControlForDirectory(filePath, &topLevel);
    if (!vc || vc->id() != Id(VcsBase::Constants::VCS_ID_GIT))
        return {};
    return topLevel;
}

bool stageFile(const FilePath &repository, const QString &relativePath)
{
    return Git::Internal::gitClient().synchronousAdd(repository, {relativePath});
}

bool unstageFile(const FilePath &repository, const QString &relativePath, VcsFileState state)
{
    bool ok = Git::Internal::gitClient().synchronousReset(repository, {relativePath});
    if (state == VcsFileState::Added) {
        ok = Git::Internal::gitClient().synchronousAdd(repository, {relativePath},
                                                       {"--intent-to-add"})
             && ok;
    }
    return ok;
}

bool checkoutFile(const FilePath &repository, const QString &relativePath, QString *errorMessage)
{
    return Git::Internal::gitClient()
        .synchronousCheckoutFiles(repository, {relativePath}, {}, errorMessage);
}

} // namespace ChangesPanel

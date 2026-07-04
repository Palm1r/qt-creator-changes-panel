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
    if (staged || state == VcsFileState::Added)
        return StageAction::Unstage;
    if (state == VcsFileState::Modified || state == VcsFileState::Untracked
        || state == VcsFileState::Deleted) {
        return StageAction::Stage;
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

void stageFile(const FilePath &repository, const QString &relativePath)
{
    Git::Internal::gitClient().addFile(repository, relativePath);
}

void unstageFile(const FilePath &repository, const QString &relativePath, VcsFileState state)
{
    Git::Internal::gitClient().synchronousReset(repository, {relativePath});
    if (state == VcsFileState::Added)
        Git::Internal::gitClient().synchronousAdd(repository, {relativePath}, {"--intent-to-add"});
}

bool checkoutFile(const FilePath &repository, const QString &relativePath, QString *errorMessage)
{
    return Git::Internal::gitClient()
        .synchronousCheckoutFiles(repository, {relativePath}, {}, errorMessage);
}

} // namespace ChangesPanel

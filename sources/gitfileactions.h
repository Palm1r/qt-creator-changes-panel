// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <coreplugin/vcsfilestate.h>

#include <QString>

namespace Utils {
class FilePath;
}

namespace ChangesPanel {

enum class StageAction {
    None,
    Stage,
    Unstage,
};

StageAction stageActionFor(Core::VcsFileState state, bool staged);
bool isRevertable(Core::VcsFileState state);

Utils::FilePath gitRepositoryFor(const Utils::FilePath &filePath);
bool stageFile(const Utils::FilePath &repository, const QString &relativePath);
bool unstageFile(
    const Utils::FilePath &repository, const QString &relativePath, Core::VcsFileState state);
bool checkoutFile(
    const Utils::FilePath &repository, const QString &relativePath, QString *errorMessage);

} // namespace ChangesPanel

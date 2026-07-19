// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <utils/osspecificaspects.h>

#include <QString>
#include <QStringList>

#include <optional>

namespace ChangesPanel {

QStringList expandedLaunchCommand(
    const QString &command,
    Utils::OsType os,
    const QString &repositoryPath,
    const std::optional<QString> &filePath = std::nullopt,
    const std::optional<QString> &relativeFilePath = std::nullopt);

} // namespace ChangesPanel

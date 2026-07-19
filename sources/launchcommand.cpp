// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "launchcommand.h"

#include <utils/commandline.h>

using namespace Utils;

namespace ChangesPanel {

QStringList expandedLaunchCommand(
    const QString &command,
    OsType os,
    const QString &repositoryPath,
    const std::optional<QString> &filePath)
{
    QStringList arguments = ProcessArgs::splitArgs(command.trimmed(), os);
    for (QString &argument : arguments) {
        argument.replace("%{repo}", repositoryPath);
        if (filePath)
            argument.replace("%{file}", *filePath);
    }
    return arguments;
}

} // namespace ChangesPanel

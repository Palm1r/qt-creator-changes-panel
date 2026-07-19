// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <QObject>

namespace ChangesPanel {

class LaunchCommandTest final : public QObject
{
    Q_OBJECT

private slots:
    void testRepositorySubstitution();
    void testFileSubstitution();
    void testPlaceholderInsideArgument();
    void testPathsWithSpacesNeedNoQuoting();
    void testCommandWithoutPlaceholders();
    void testFilePlaceholderUntouchedWithoutFile();
    void testUnknownPlaceholderUntouched();
    void testEmptyCommand();
};

} // namespace ChangesPanel

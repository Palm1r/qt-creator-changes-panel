// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "launchcommandtest.h"

#include "launchcommand.h"

#include <QTest>

using namespace Utils;

namespace ChangesPanel {

void LaunchCommandTest::testRepositorySubstitution()
{
    const QStringList arguments
        = expandedLaunchCommand("smerge %{repo}", OsTypeLinux, "/home/user/project");
    QCOMPARE(arguments, QStringList({"smerge", "/home/user/project"}));
}

void LaunchCommandTest::testFileSubstitution()
{
    const QStringList arguments = expandedLaunchCommand(
        "client --repo %{repo} --file %{file}",
        OsTypeLinux,
        "/home/user/project",
        QString("/home/user/project/src/main.cpp"));
    QCOMPARE(
        arguments,
        QStringList(
            {"client", "--repo", "/home/user/project", "--file",
             "/home/user/project/src/main.cpp"}));
}

void LaunchCommandTest::testPlaceholderInsideArgument()
{
    const QStringList arguments
        = expandedLaunchCommand("tool /path:%{repo} /command:repostatus", OsTypeLinux, "/repo");
    QCOMPARE(arguments, QStringList({"tool", "/path:/repo", "/command:repostatus"}));
}

void LaunchCommandTest::testPathsWithSpacesNeedNoQuoting()
{
    const QStringList arguments
        = expandedLaunchCommand("smerge %{repo}", OsTypeLinux, "/home/user/my project");
    QCOMPARE(arguments, QStringList({"smerge", "/home/user/my project"}));
}

void LaunchCommandTest::testCommandWithoutPlaceholders()
{
    const QStringList arguments = expandedLaunchCommand("gitk --all", OsTypeLinux, "/repo");
    QCOMPARE(arguments, QStringList({"gitk", "--all"}));
}

void LaunchCommandTest::testFilePlaceholderUntouchedWithoutFile()
{
    const QStringList arguments = expandedLaunchCommand("smerge %{file}", OsTypeLinux, "/repo");
    QCOMPARE(arguments, QStringList({"smerge", "%{file}"}));
}

void LaunchCommandTest::testUnknownPlaceholderUntouched()
{
    const QStringList arguments = expandedLaunchCommand(
        "smerge %{repos} %{repo}", OsTypeLinux, "/repo", QString("/repo/file"));
    QCOMPARE(arguments, QStringList({"smerge", "%{repos}", "/repo"}));
}

void LaunchCommandTest::testEmptyCommand()
{
    QCOMPARE(expandedLaunchCommand({}, OsTypeLinux, "/repo"), QStringList());
    QCOMPARE(expandedLaunchCommand("   ", OsTypeLinux, "/repo"), QStringList());
}

} // namespace ChangesPanel

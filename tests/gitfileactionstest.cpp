// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "gitfileactionstest.h"

#include "fakegitcommands.h"
#include "gitfileactions.h"

#include <utils/filepath.h>

#include <QSignalSpy>
#include <QTest>

using namespace Utils;

namespace ChangesPanel {

static StageableFile changeAt(
    const FilePath &repository, const QString &relativePath, FileState state)
{
    return {repository, relativePath, state};
}

static QList<FilePath> refreshedRepositories(const QSignalSpy &spy)
{
    QList<FilePath> repositories;
    for (const QList<QVariant> &arguments : spy)
        repositories.append(arguments.at(0).value<FilePath>());
    return repositories;
}

void GitFileActionsTest::testNoopActionsSkipGit()
{
    FakeGitCommands git;
    GitFileActions actions(git);
    QSignalSpy refreshes(&actions, &GitFileActions::refreshRequested);

    QVERIFY(actions.applyStageAction(
        StageAction::None,
        {changeAt(FilePath::fromString("/repo"), "a.cpp", FileState::Modified)}));
    QVERIFY(actions.applyStageAction(StageAction::Stage, {}));

    QCOMPARE(git.mutationCount(), 0);
    QCOMPARE(refreshes.size(), 0);
}

void GitFileActionsTest::testStageBatchesPerRepository()
{
    FakeGitCommands git;
    GitFileActions actions(git);
    const FilePath first = FilePath::fromString("/first");
    const FilePath second = FilePath::fromString("/second");

    QVERIFY(actions.applyStageAction(
        StageAction::Stage,
        {changeAt(first, "a.cpp", FileState::Modified),
         changeAt(second, "b.cpp", FileState::Modified),
         changeAt(first, "c.cpp", FileState::Untracked)}));

    QCOMPARE(git.stageCalls.size(), 2);
    const FakeGitCommands::PathsCall *firstCall = git.stageCallFor(first);
    const FakeGitCommands::PathsCall *secondCall = git.stageCallFor(second);
    QVERIFY(firstCall);
    QVERIFY(secondCall);
    QCOMPARE(firstCall->relativePaths, QStringList({"a.cpp", "c.cpp"}));
    QCOMPARE(secondCall->relativePaths, QStringList({"b.cpp"}));
}

void GitFileActionsTest::testUnstageRestoresIntentToAdd()
{
    FakeGitCommands git;
    GitFileActions actions(git);
    const FilePath repository = FilePath::fromString("/repo");

    QVERIFY(actions.applyStageAction(
        StageAction::Unstage,
        {changeAt(repository, "added.cpp", FileState::Added),
         changeAt(repository, "modified.cpp", FileState::Modified)}));

    QCOMPARE(git.stageCalls.size(), 0);
    QCOMPARE(git.unstageCalls.size(), 1);
    const FakeGitCommands::PathsCall &call = git.unstageCalls.first();
    QCOMPARE(call.relativePaths, QStringList({"added.cpp", "modified.cpp"}));
    QCOMPARE(call.intentToAddPaths, QStringList({"added.cpp"}));
}

void GitFileActionsTest::testFailureAggregatesAndStillRefreshes()
{
    FakeGitCommands git;
    const FilePath failing = FilePath::fromString("/failing");
    const FilePath healthy = FilePath::fromString("/healthy");
    git.failures.insert(failing, "the git add command failed");

    GitFileActions actions(git);
    QSignalSpy refreshes(&actions, &GitFileActions::refreshRequested);

    const Result<> result = actions.applyStageAction(
        StageAction::Stage,
        {changeAt(failing, "a.cpp", FileState::Modified),
         changeAt(healthy, "b.cpp", FileState::Modified)});

    QVERIFY(!result);
    QVERIFY(result.error().contains("the git add command failed"));
    QCOMPARE(git.stageCalls.size(), 2);

    const QList<FilePath> refreshed = refreshedRepositories(refreshes);
    QCOMPARE(refreshed.size(), 2);
    QVERIFY(refreshed.contains(failing));
    QVERIFY(refreshed.contains(healthy));
}

void GitFileActionsTest::testRefreshFollowsEveryMutation()
{
    FakeGitCommands git;
    GitFileActions actions(git);
    const FilePath repository = FilePath::fromString("/repo");
    QSignalSpy refreshes(&actions, &GitFileActions::refreshRequested);

    QVERIFY(actions.applyStageAction(
        StageAction::Stage, {changeAt(repository, "a.cpp", FileState::Modified)}));
    QCOMPARE(refreshes.size(), 1);

    QVERIFY(actions.applyStageAction(
        StageAction::Unstage, {changeAt(repository, "a.cpp", FileState::Modified)}));
    QCOMPARE(refreshes.size(), 2);

    QVERIFY(actions.revertFile(repository, "a.cpp"));
    QCOMPARE(refreshes.size(), 3);

    QCOMPARE(refreshedRepositories(refreshes), QList<FilePath>({repository, repository, repository}));
}

void GitFileActionsTest::testRevertIgnoresEmptyList()
{
    FakeGitCommands git;
    GitFileActions actions(git);
    QSignalSpy refreshes(&actions, &GitFileActions::refreshRequested);

    QVERIFY(actions.revertFiles(FilePath::fromString("/repo"), {}));

    QCOMPARE(git.checkoutCalls.size(), 0);
    QCOMPARE(refreshes.size(), 0);
}

void GitFileActionsTest::testRevertErrorNamesSingleFile()
{
    FakeGitCommands git;
    const FilePath repository = FilePath::fromString("/repo");
    git.failures.insert(repository, "checkout refused");
    GitFileActions actions(git);

    const Result<> single = actions.revertFile(repository, "sub/lonely.cpp");
    QVERIFY(!single);
    QVERIFY(single.error().contains("lonely.cpp"));
    QVERIFY(single.error().contains("checkout refused"));

    const Result<> many = actions.revertFiles(repository, {"a.cpp", "b.cpp"});
    QVERIFY(!many);
    QVERIFY(!many.error().contains("a.cpp"));
    QVERIFY(many.error().contains("checkout refused"));
}

} // namespace ChangesPanel

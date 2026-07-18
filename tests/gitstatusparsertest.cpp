// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "gitstatusparsertest.h"

#include "gitstatusparser.h"

#include <QTest>

namespace ChangesPanel {

void GitStatusParserTest::testStatusStates()
{
    const GitStatus status = parseStatusOutput(
        "## main\n"
        " M modified.cpp\n"
        "?? untracked.txt\n"
        "A  added.h\n"
        " D deleted.md\n"
        " T typechange.bin\n");
    QCOMPARE(status.fileStates.size(), 5);
    QCOMPARE(status.fileStates.value("modified.cpp"), FileState::Modified);
    QCOMPARE(status.fileStates.value("untracked.txt"), FileState::Untracked);
    QCOMPARE(status.fileStates.value("added.h"), FileState::Added);
    QCOMPARE(status.fileStates.value("deleted.md"), FileState::Deleted);
    QCOMPARE(status.fileStates.value("typechange.bin"), FileState::Modified);
}

void GitStatusParserTest::testStatusStagedDetection()
{
    const GitStatus status = parseStatusOutput(
        "M  staged.cpp\n"
        "MM partially.cpp\n"
        " M worktreeonly.cpp\n"
        "?? untracked.txt\n");
    QVERIFY(status.stagedFiles.contains("staged.cpp"));
    QVERIFY(status.stagedFiles.contains("partially.cpp"));
    QVERIFY(!status.stagedFiles.contains("worktreeonly.cpp"));
    QVERIFY(!status.stagedFiles.contains("untracked.txt"));
}

void GitStatusParserTest::testStatusUnmergedCodes()
{
    const GitStatus status = parseStatusOutput(
        "UU both-modified.cpp\n"
        "AA both-added.cpp\n"
        "DD both-deleted.cpp\n"
        "AU added-by-us.cpp\n"
        "DU deleted-by-us.cpp\n");
    const QStringList files = {"both-modified.cpp", "both-added.cpp", "both-deleted.cpp",
                               "added-by-us.cpp", "deleted-by-us.cpp"};
    for (const QString &file : files) {
        QCOMPARE(status.fileStates.value(file), FileState::Unmerged);
        QVERIFY(!status.stagedFiles.contains(file));
    }
}

void GitStatusParserTest::testStatusRenames()
{
    const GitStatus status = parseStatusOutput(
        "R  old.cpp -> new.cpp\n"
        "C  base.h -> copy.h\n");
    QCOMPARE(status.fileStates.value("new.cpp"), FileState::Renamed);
    QCOMPARE(status.fileStates.value("copy.h"), FileState::Renamed);
    QVERIFY(!status.fileStates.contains("old.cpp"));
    QVERIFY(status.stagedFiles.contains("new.cpp"));
}

void GitStatusParserTest::testStatusQuotedPaths()
{
    const GitStatus status = parseStatusOutput(
        " M \"with\\tтаб.txt\"\n"
        "R  \"old \\\"q\\\".txt\" -> \"new \\\"q\\\".txt\"\n");
    QCOMPARE(status.fileStates.value("with\tтаб.txt"), FileState::Modified);
    QCOMPARE(status.fileStates.value("new \"q\".txt"), FileState::Renamed);
}

void GitStatusParserTest::testStatusIgnoresShortAndCrLfLines()
{
    const GitStatus status = parseStatusOutput(
        "## main\r\n"
        " M windows.txt\r\n"
        "XY\n"
        "MMMM\n"
        "MMxgarbage.txt\n");
    QCOMPARE(status.fileStates.size(), 1);
    QCOMPARE(status.fileStates.value("windows.txt"), FileState::Modified);
}

void GitStatusParserTest::testBranchHeader_data()
{
    QTest::addColumn<QString>("output");
    QTest::addColumn<QString>("branch");
    QTest::addColumn<int>("ahead");
    QTest::addColumn<int>("behind");
    QTest::addColumn<bool>("hasUpstream");

    QTest::newRow("ahead-behind")
        << "## main...origin/main [ahead 1, behind 2]\n M f.txt\n"
        << "main" << 1 << 2 << true;
    QTest::newRow("in-sync") << "## main...origin/main\n" << "main" << 0 << 0 << true;
    QTest::newRow("no-upstream") << "## feature\n" << "feature" << -1 << -1 << false;
    QTest::newRow("detached") << "## HEAD (no branch)\n" << "HEAD" << -1 << -1 << false;
    QTest::newRow("no-commits") << "## No commits yet on main\n" << "main" << -1 << -1 << false;
    QTest::newRow("empty") << "" << "" << -1 << -1 << false;
}

void GitStatusParserTest::testBranchHeader()
{
    QFETCH(QString, output);
    QFETCH(QString, branch);
    QFETCH(int, ahead);
    QFETCH(int, behind);
    QFETCH(bool, hasUpstream);

    const BranchInfo info = parseBranchHeader(output);
    QCOMPARE(info.branch, branch);
    QCOMPARE(info.ahead, ahead);
    QCOMPARE(info.behind, behind);
    QCOMPARE(info.hasUpstream, hasUpstream);
}

void GitStatusParserTest::testSubmoduleStatusLines()
{
    const QStringList submodules = parseSubmoduleStatusLines(
        {" 1234abcd libs/inner (v1.0)",
         "-0000dead libs/uninitialized",
         "+5678beef tools/dirty (heads/main)",
         " 9abc0123 libs/my module (v2.0)",
         " 4def5678 plain/path",
         " badline",
         ""});
    QCOMPARE(submodules,
             QStringList({"libs/inner", "tools/dirty", "libs/my module", "plain/path"}));
}

void GitStatusParserTest::testUnquote_data()
{
    QTest::addColumn<QString>("quoted");
    QTest::addColumn<QString>("expected");

    QTest::newRow("plain") << "path/file.txt" << "path/file.txt";
    QTest::newRow("control-chars") << "\"a\\tb\\nc\"" << "a\tb\nc";
    QTest::newRow("escaped-quote") << "\"say \\\"hi\\\"\"" << "say \"hi\"";
    QTest::newRow("escaped-backslash") << "\"a\\\\b\"" << "a\\b";
    QTest::newRow("octal-utf8") << "\"\\320\\260\\tx\"" << QString::fromUtf8("а\tx");
    QTest::newRow("octal-single") << "\"\\7bell\"" << "\abell";
    QTest::newRow("unterminated") << "\"open" << "\"open";
}

void GitStatusParserTest::testUnquote()
{
    QFETCH(QString, quoted);
    QFETCH(QString, expected);
    QCOMPARE(unquoteGitPath(quoted), expected);
}

void GitStatusParserTest::testSplitRename_data()
{
    QTest::addColumn<QString>("pathPart");
    QTest::addColumn<QStringList>("expected");

    QTest::newRow("plain") << "old.cpp -> new.cpp" << QStringList({"old.cpp", "new.cpp"});
    QTest::newRow("both-quoted")
        << "\"o l\\td\" -> \"n e\\tw\"" << QStringList({"o l\td", "n e\tw"});
    QTest::newRow("only-old-quoted")
        << "\"o\\\"ld\" -> new.cpp" << QStringList({"o\"ld", "new.cpp"});
    QTest::newRow("only-new-quoted")
        << "old.cpp -> \"n\\\"ew\"" << QStringList({"old.cpp", "n\"ew"});
    QTest::newRow("no-arrow") << "just-a-file.cpp" << QStringList();
    QTest::newRow("quoted-without-arrow") << "\"quoted.cpp\"" << QStringList();
}

void GitStatusParserTest::testSplitRename()
{
    QFETCH(QString, pathPart);
    QFETCH(QStringList, expected);
    QCOMPARE(splitRenameLine(pathPart), expected);
}

} // namespace ChangesPanel

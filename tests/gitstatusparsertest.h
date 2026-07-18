// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <QObject>

namespace ChangesPanel {

class GitStatusParserTest final : public QObject
{
    Q_OBJECT

private slots:
    void testStatusStates();
    void testStatusStagedDetection();
    void testStatusUnmergedCodes();
    void testStatusRenames();
    void testStatusQuotedPaths();
    void testStatusIgnoresShortAndCrLfLines();
    void testBranchHeader_data();
    void testBranchHeader();
    void testSubmoduleStatusLines();
    void testUnquote_data();
    void testUnquote();
    void testSplitRename_data();
    void testSplitRename();
};

} // namespace ChangesPanel

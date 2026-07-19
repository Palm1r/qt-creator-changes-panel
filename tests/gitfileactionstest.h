// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <QObject>

namespace ChangesPanel {

class GitFileActionsTest final : public QObject
{
    Q_OBJECT

private slots:
    void testNoopActionsSkipGit();
    void testStageBatchesPerRepository();
    void testUnstageRestoresIntentToAdd();
    void testFailureAggregatesAndStillRefreshes();
    void testRefreshFollowsEveryMutation();
    void testRevertIgnoresEmptyList();
    void testRevertErrorNamesSingleFile();
};

} // namespace ChangesPanel

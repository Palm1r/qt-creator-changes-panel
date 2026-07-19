// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <QObject>

namespace ChangesPanel {

class ChangedDocumentsModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void testGroupAssignment_data();
    void testGroupAssignment();
    void testStagedSetMigration();
    void testRemovalBatching();
    void testClearRepositoryIsScoped();
    void testRepositoriesAreIsolated();
    void testIgnoredPaths_data();
    void testIgnoredPaths();
};

} // namespace ChangesPanel

// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "changeddocumentsmodeltest.h"

#include "changeddocumentsmodel.h"
#include "gitstatusparser.h"

#include <utils/filepath.h>

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTest>

#include <initializer_list>
#include <utility>

using namespace Utils;

namespace ChangesPanel {

static GitStatus makeStatus(
    std::initializer_list<std::pair<QString, FileState>> files, const QStringList &staged = {})
{
    GitStatus status;
    for (const auto &[relativePath, state] : files)
        status.fileStates.insert(relativePath, state);
    for (const QString &relativePath : staged)
        status.stagedFiles.insert(relativePath);
    return status;
}

static int rowsIn(const ChangedDocumentsModel &model, ChangedDocumentsModel::Group group)
{
    return model.rowCount(model.index(group, 0));
}

static QString relativePathOf(
    const ChangedDocumentsModel &model, ChangedDocumentsModel::Group group, int row)
{
    const QModelIndex index = model.index(row, 0, model.index(group, 0));
    return index.data(ChangedDocumentsModel::RelativePathRole).toString();
}

static FilePath repositoryOf(
    const ChangedDocumentsModel &model, ChangedDocumentsModel::Group group, int row)
{
    const QModelIndex index = model.index(row, 0, model.index(group, 0));
    return FilePath::fromVariant(index.data(ChangedDocumentsModel::RepositoryRole));
}

void ChangedDocumentsModelTest::testGroupAssignment_data()
{
    QTest::addColumn<int>("state");
    QTest::addColumn<bool>("staged");
    QTest::addColumn<int>("expectedGroup");

    QTest::newRow("unmerged") << int(FileState::Unmerged) << false
                              << int(ChangedDocumentsModel::MergeGroup);
    QTest::newRow("unmerged-outranks-staged")
        << int(FileState::Unmerged) << true << int(ChangedDocumentsModel::MergeGroup);
    QTest::newRow("staged") << int(FileState::Modified) << true
                            << int(ChangedDocumentsModel::StagedGroup);
    QTest::newRow("unstaged") << int(FileState::Modified) << false
                              << int(ChangedDocumentsModel::UnstagedGroup);
    QTest::newRow("untracked") << int(FileState::Untracked) << false
                               << int(ChangedDocumentsModel::UnstagedGroup);
}

void ChangedDocumentsModelTest::testGroupAssignment()
{
    QFETCH(int, state);
    QFETCH(bool, staged);
    QFETCH(int, expectedGroup);

    ChangedDocumentsModel model;
    QAbstractItemModelTester tester(&model);

    model.applyStatus(
        FilePath::fromString("/repo"),
        makeStatus({{"file.cpp", FileState(state)}}, staged ? QStringList{"file.cpp"} : QStringList{}));

    for (int group = 0; group < ChangedDocumentsModel::GroupCount; ++group) {
        QCOMPARE(
            model.rowCount(model.index(group, 0)), group == expectedGroup ? 1 : 0);
    }
}

void ChangedDocumentsModelTest::testStagedSetMigration()
{
    ChangedDocumentsModel model;
    QAbstractItemModelTester tester(&model);
    const FilePath repository = FilePath::fromString("/repo");

    model.applyStatus(repository, makeStatus({{"a.cpp", FileState::Modified}}, {"a.cpp"}));
    QCOMPARE(rowsIn(model, ChangedDocumentsModel::StagedGroup), 1);
    QCOMPARE(rowsIn(model, ChangedDocumentsModel::UnstagedGroup), 0);

    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);

    model.applyStatus(repository, makeStatus({{"a.cpp", FileState::Modified}}));

    QCOMPARE(rowsIn(model, ChangedDocumentsModel::StagedGroup), 0);
    QCOMPARE(rowsIn(model, ChangedDocumentsModel::UnstagedGroup), 1);
    QCOMPARE(removed.size(), 1);
    QCOMPARE(inserted.size(), 1);
    QCOMPARE(
        removed.first().at(0).value<QModelIndex>().row(),
        int(ChangedDocumentsModel::StagedGroup));
    QCOMPARE(
        inserted.first().at(0).value<QModelIndex>().row(),
        int(ChangedDocumentsModel::UnstagedGroup));
}

void ChangedDocumentsModelTest::testRemovalBatching()
{
    ChangedDocumentsModel model;
    QAbstractItemModelTester tester(&model);
    const FilePath repository = FilePath::fromString("/repo");

    model.applyStatus(
        repository,
        makeStatus({{"a.cpp", FileState::Modified},
                    {"b.cpp", FileState::Modified},
                    {"c.cpp", FileState::Modified},
                    {"d.cpp", FileState::Modified},
                    {"e.cpp", FileState::Modified}}));
    QCOMPARE(rowsIn(model, ChangedDocumentsModel::UnstagedGroup), 5);

    QSignalSpy scattered(&model, &QAbstractItemModel::rowsRemoved);
    model.applyStatus(
        repository,
        makeStatus({{"b.cpp", FileState::Modified}, {"d.cpp", FileState::Modified}}));

    QCOMPARE(rowsIn(model, ChangedDocumentsModel::UnstagedGroup), 2);
    QCOMPARE(scattered.size(), 3);
    QCOMPARE(scattered.at(0).at(1).toInt(), 4);
    QCOMPARE(scattered.at(0).at(2).toInt(), 4);
    QCOMPARE(scattered.at(1).at(1).toInt(), 2);
    QCOMPARE(scattered.at(1).at(2).toInt(), 2);
    QCOMPARE(scattered.at(2).at(1).toInt(), 0);
    QCOMPARE(scattered.at(2).at(2).toInt(), 0);

    QSignalSpy adjacent(&model, &QAbstractItemModel::rowsRemoved);
    model.applyStatus(repository, makeStatus({}));

    QCOMPARE(rowsIn(model, ChangedDocumentsModel::UnstagedGroup), 0);
    QCOMPARE(adjacent.size(), 1);
    QCOMPARE(adjacent.first().at(1).toInt(), 0);
    QCOMPARE(adjacent.first().at(2).toInt(), 1);
}

void ChangedDocumentsModelTest::testClearRepositoryIsScoped()
{
    ChangedDocumentsModel model;
    QAbstractItemModelTester tester(&model);
    const FilePath first = FilePath::fromString("/first");
    const FilePath second = FilePath::fromString("/second");

    model.applyStatus(
        first,
        makeStatus({{"a.cpp", FileState::Modified}, {"b.cpp", FileState::Modified}}));
    model.applyStatus(second, makeStatus({{"c.cpp", FileState::Modified}}));
    QCOMPARE(rowsIn(model, ChangedDocumentsModel::UnstagedGroup), 3);

    model.clearRepository(first);

    QCOMPARE(rowsIn(model, ChangedDocumentsModel::UnstagedGroup), 1);
    QCOMPARE(relativePathOf(model, ChangedDocumentsModel::UnstagedGroup, 0), QString("c.cpp"));
    QCOMPARE(repositoryOf(model, ChangedDocumentsModel::UnstagedGroup, 0), second);
}

void ChangedDocumentsModelTest::testRepositoriesAreIsolated()
{
    ChangedDocumentsModel model;
    QAbstractItemModelTester tester(&model);
    const FilePath first = FilePath::fromString("/first");
    const FilePath second = FilePath::fromString("/second");

    model.applyStatus(first, makeStatus({{"same.cpp", FileState::Modified}}));
    model.applyStatus(second, makeStatus({{"same.cpp", FileState::Modified}}));
    QCOMPARE(rowsIn(model, ChangedDocumentsModel::UnstagedGroup), 2);

    model.applyStatus(first, makeStatus({}));

    QCOMPARE(rowsIn(model, ChangedDocumentsModel::UnstagedGroup), 1);
    QCOMPARE(repositoryOf(model, ChangedDocumentsModel::UnstagedGroup, 0), second);
}

void ChangedDocumentsModelTest::testIgnoredPaths_data()
{
    QTest::addColumn<QString>("relativePath");

    QTest::newRow("empty") << QString();
    QTest::newRow("directory") << QStringLiteral("sub/");
    QTest::newRow("outside-repository") << QStringLiteral("../outside.cpp");
}

void ChangedDocumentsModelTest::testIgnoredPaths()
{
    QFETCH(QString, relativePath);

    ChangedDocumentsModel model;
    QAbstractItemModelTester tester(&model);

    model.applyStatus(
        FilePath::fromString("/repo"), makeStatus({{relativePath, FileState::Modified}}));

    for (int group = 0; group < ChangedDocumentsModel::GroupCount; ++group)
        QCOMPARE(model.rowCount(model.index(group, 0)), 0);
}

} // namespace ChangesPanel

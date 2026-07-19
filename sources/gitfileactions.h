// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include "gitstatusparser.h"

#include <utils/filepath.h>
#include <utils/result.h>

#include <QList>
#include <QObject>
#include <QString>

namespace ChangesPanel {

class GitCommands;

enum class StageAction {
    None,
    Stage,
    Unstage,
};

enum class FileEntryAction {
    None,
    OpenEditor,
    ShowDiff,
};

struct StageableFile
{
    Utils::FilePath repository;
    QString relativePath;
    FileState state = FileState::Unknown;
};

StageAction stageActionFor(FileState state, bool staged);
QString stageActionToolTip(StageAction action, const QString &fileName);
QString stageActionFailureTitle(StageAction action);
bool isRevertable(FileState state);

FileEntryAction diffColumnActionFor(FileState state);
FileEntryAction rowClickActionFor(FileState state);
QString fileEntryActionToolTip(FileEntryAction action, const QString &fileName);

class GitFileActions final : public QObject
{
    Q_OBJECT

public:
    explicit GitFileActions(GitCommands &git, QObject *parent = nullptr);

    Utils::Result<> applyStageAction(StageAction action, const QList<StageableFile> &files);
    Utils::Result<> revertFile(const Utils::FilePath &repository, const QString &relativePath);
    Utils::Result<> revertFiles(const Utils::FilePath &repository,
                                const QStringList &relativePaths);

    void diffFile(const Utils::FilePath &repository, const QString &relativePath, bool staged);
    void diffAllChanges(const Utils::FilePath &repository,
                        const QStringList &unstagedPaths,
                        const QStringList &stagedPaths);
    void openRepositoryInGitClient(const Utils::FilePath &repository);
    void openFileInGitClient(const Utils::FilePath &repository, const QString &relativePath);

signals:
    void refreshRequested(const Utils::FilePath &repository);

private:
    GitCommands &m_git;
};

} // namespace ChangesPanel

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
class GitStatusTracker;

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
    GitFileActions(GitCommands &git, GitStatusTracker &tracker, QObject *parent = nullptr);

    Utils::Result<> applyStageAction(StageAction action, const QList<StageableFile> &files);
    Utils::Result<> revertFile(const Utils::FilePath &repository, const QString &relativePath);

    void diffFile(const Utils::FilePath &repository, const QString &relativePath, bool staged);
    void openRepositoryInGitClient(const Utils::FilePath &repository);
    void openFileInGitClient(const Utils::FilePath &repository, const QString &relativePath);

private:
    GitCommands &m_git;
    GitStatusTracker &m_tracker;
};

} // namespace ChangesPanel

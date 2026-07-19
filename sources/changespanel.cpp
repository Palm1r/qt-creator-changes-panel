// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "changeddocumentsmodel.h"
#include "changeddocumentsviewfactory.h"
#include "changespanelsettings.h"
#include "changespaneltr.h"
#include "gitcommands.h"
#include "gitfileactions.h"
#include "gitstatustracker.h"

#include <coreplugin/icore.h>

#include <extensionsystem/iplugin.h>

#include <QCoreApplication>
#include <QTranslator>

#include <memory>

#ifdef WITH_TESTS
#include "changeddocumentsmodeltest.h"
#include "gitstatusparsertest.h"
#include "launchcommandtest.h"
#endif

namespace ChangesPanel {

class ChangesPanelPlugin final : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "ChangesPanel.json")

public:
    void initialize() final
    {
        installTranslator();

#ifdef WITH_TESTS
        addTest<ChangedDocumentsModelTest>();
        addTest<GitStatusParserTest>();
        addTest<LaunchCommandTest>();
#endif

        if (settings().enabled())
            setupPanel();

        connect(&settings().enabled, &Utils::BaseAspect::changed, this, [] {
            if (Core::ICore::askForRestart(
                    Tr::tr("Enabling or disabling the Changes panel takes effect after "
                           "restarting Qt Creator."))) {
                Core::ICore::restart();
            }
        });
    }

    ShutdownFlag aboutToShutdown() final
    {
        if (m_translator) {
            QCoreApplication::removeTranslator(m_translator);
            m_translator = nullptr;
        }
        if (m_viewFactory) {
            m_viewFactory.reset();
            m_tracker->detachFromExternalSources();
        }
        return SynchronousShutdown;
    }

private:
    void setupPanel()
    {
        m_tracker = new GitStatusTracker(gitCommands(), this);
        m_model = new ChangedDocumentsModel(this);
        connect(
            m_tracker,
            &GitStatusTracker::statusChanged,
            m_model,
            &ChangedDocumentsModel::applyStatus);
        connect(
            m_tracker,
            &GitStatusTracker::repositoryCleared,
            m_model,
            &ChangedDocumentsModel::clearRepository);

        m_actions = new GitFileActions(gitCommands(), *m_tracker, this);
        m_viewFactory
            = std::make_unique<ChangedDocumentsViewFactory>(m_model, m_tracker, m_actions);
    }

    void installTranslator()
    {
        auto translator = new QTranslator(this);
        if (translator->load(
                QLocale(Core::ICore::userInterfaceLanguage()),
                "ChangesPanel",
                "_",
                ":/translations")) {
            QCoreApplication::installTranslator(translator);
            m_translator = translator;
        } else {
            delete translator;
        }
    }

    GitStatusTracker *m_tracker = nullptr;
    ChangedDocumentsModel *m_model = nullptr;
    GitFileActions *m_actions = nullptr;
    QTranslator *m_translator = nullptr;
    std::unique_ptr<ChangedDocumentsViewFactory> m_viewFactory;
};

} // namespace ChangesPanel

#include <changespanel.moc>

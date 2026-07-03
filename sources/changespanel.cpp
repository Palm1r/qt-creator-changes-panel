// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "changeddocumentsmodel.h"
#include "changeddocumentsviewfactory.h"

#include <coreplugin/icore.h>
#include <coreplugin/vcsmanager.h>

#include <projectexplorer/projectmanager.h>

#include <extensionsystem/iplugin.h>

#include <QCoreApplication>
#include <QTranslator>

#include <memory>

namespace ChangesPanel {

class ChangesPanelPlugin final : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "ChangesPanel.json")

public:
    void initialize() final
    {
        installTranslator();

        m_model = new ChangedDocumentsModel(this);
        m_viewFactory = std::make_unique<ChangedDocumentsViewFactory>(m_model);
    }

    ShutdownFlag aboutToShutdown() final
    {
        if (m_translator) {
            QCoreApplication::removeTranslator(m_translator);
            m_translator = nullptr;
        }
        m_viewFactory.reset();
        disconnect(Core::VcsManager::instance(), nullptr, m_model, nullptr);
        disconnect(ProjectExplorer::ProjectManager::instance(), nullptr, m_model, nullptr);
        return SynchronousShutdown;
    }

private:
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

    ChangedDocumentsModel *m_model = nullptr;
    QTranslator *m_translator = nullptr;
    std::unique_ptr<ChangedDocumentsViewFactory> m_viewFactory;
};

} // namespace ChangesPanel

#include <changespanel.moc>

cmake_minimum_required(VERSION 3.16)

project(ChangesPanel)

set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(QtCreator REQUIRED COMPONENTS Core)
find_package(Qt6 6.9 REQUIRED COMPONENTS Widgets LinguistTools)

qt_standard_project_setup(I18N_TRANSLATED_LANGUAGES
    en cs zh_CN zh_TW da de fr hr ja pl ru sl sv uk
)

option(WITH_TESTS "Builds with tests" NO)

if(WITH_TESTS)
  find_package(Qt6 REQUIRED COMPONENTS Test)
  enable_testing()
endif()

add_qtc_plugin(ChangesPanel
  PLUGIN_DEPENDS
    QtCreator::Core
    QtCreator::Git
    QtCreator::ProjectExplorer
  DEPENDS
    Qt::Widgets
    QtCreator::ExtensionSystem
    QtCreator::Utils
  SOURCES
    .github/workflows/build_cmake.yml
    .github/workflows/README.md
    README.md
    resources/changespanel.qrc
    sources/changeddocumentsdelegate.cpp
    sources/changeddocumentsdelegate.h
    sources/changeddocumentsmodel.cpp
    sources/changeddocumentsmodel.h
    sources/changeddocumentsproxymodel.cpp
    sources/changeddocumentsproxymodel.h
    sources/changeddocumentsviewfactory.cpp
    sources/changeddocumentsviewfactory.h
    sources/changeddocumentswidget.cpp
    sources/changeddocumentswidget.h
    sources/changespanel.cpp
    sources/changespanelconstants.h
    sources/changespaneltr.h
    sources/gitfileactions.cpp
    sources/gitfileactions.h
    sources/gitstatusparser.cpp
    sources/gitstatusparser.h
    sources/gitstatustracker.cpp
    sources/gitstatustracker.h
)

target_include_directories(ChangesPanel PRIVATE sources)

extend_qtc_plugin(ChangesPanel
  CONDITION WITH_TESTS
  DEPENDS Qt::Test
  SOURCES
    tests/gitstatusparsertest.cpp
    tests/gitstatusparsertest.h
)

if(WITH_TESTS)
  target_include_directories(ChangesPanel PRIVATE tests)
endif()

get_target_property(QtCreatorCorePath QtCreator::Core LOCATION)
find_program(QtCreatorExecutable
  NAMES
    qtcreator "Qt Creator"
  PATHS
    "${QtCreatorCorePath}/../../../bin"
    "${QtCreatorCorePath}/../../../MacOS"
  NO_DEFAULT_PATH
)
if (QtCreatorExecutable)
  add_custom_target(RunQtCreator
    COMMAND ${QtCreatorExecutable} -pluginpath $<TARGET_FILE_DIR:ChangesPanel>
    DEPENDS ChangesPanel
  )
  set_target_properties(RunQtCreator PROPERTIES FOLDER "qtc_runnable")
endif()

qt_add_translations(TARGETS ChangesPanel
          TS_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/resources/translations
          RESOURCE_PREFIX "/translations"
          LUPDATE_OPTIONS -no-obsolete -locations none
)

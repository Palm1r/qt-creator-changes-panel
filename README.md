# Qt Creator Changes Panel

A Qt Creator plugin that adds a **Changes** navigation pane — a sibling of
"Open Documents" that lists the files currently modified according to version control
(git status: modified, added, deleted, renamed, unmerged, untracked) across the open
projects' repositories.

## Features

- Lists all VCS-changed files of the open projects, updated live (same data source
  that colors files in the Projects tree).
- File name on the left, its directory (relative to the repository) dimmed on the right.
- Files are colored by their VCS state, with the state shown in the tooltip.
- Single click opens the file in the editor.
- Context menu: Open, Open Containing Folder, Copy Full Path.
- Toolbar filter to show/hide untracked files (persisted in settings).
- Activation shortcut: `Meta+G` (macOS) / `Alt+G`.

The pane relies on Qt Creator's VCS status monitoring: make sure
**"Show file status"** is enabled in *Preferences > Version Control > General*
(it is enabled by default).

Requires Qt Creator 20 (uses the `Core::VcsManager` file-state API).

## How to Build

Create a build directory and run

    cmake -DCMAKE_PREFIX_PATH=<path_to_qtcreator> -DCMAKE_BUILD_TYPE=RelWithDebInfo <path_to_plugin_source>
    cmake --build .

where `<path_to_qtcreator>` is the relative or absolute path to a Qt Creator build directory, or to a
combined binary and development package (Windows / Linux), or to the `Qt Creator.app/Contents/Resources/`
directory of a combined binary and development package (macOS), and `<path_to_plugin_source>` is the
relative or absolute path to this plugin directory.

## How to Run

From the command line run

    cmake --build . --target RunQtCreator

`RunQtCreator` is a custom CMake target that will use the <path to qtcreator> referenced above to
start the Qt Creator executable with the following parameters

    -pluginpath <path_to_plugin>

where `<path_to_plugin>` is the path to the resulting plugin library in the build directory
(`<plugin_build>/lib/qtcreator/plugins` on Windows and Linux,
`<plugin_build>/Qt Creator.app/Contents/PlugIns` on macOS).

You might want to add `-temporarycleansettings` (or `-tcs`) to ensure that the opened Qt Creator
instance cannot mess with your user-global Qt Creator settings.

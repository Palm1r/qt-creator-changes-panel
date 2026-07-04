# Qt Creator Changes Panel

A Qt Creator plugin that adds a **Changes** navigation pane — a sibling of
"Open Documents" that lists the files currently modified according to version control
(git status: modified, added, deleted, renamed, unmerged, untracked) across the open
projects' repositories.

## Features

- Lists all VCS-changed files of the open projects, updated live (same data source
  that colors files in the Projects tree).
- Files are grouped into collapsible **Merge Changes**, **Staged Changes** and
  **Unstaged Changes** sections with file counts.
- Hover actions on each file: diff, revert (or recover a deleted file),
  stage/unstage.
- File name on the left, its directory (relative to the repository) dimmed on the right.
- Files are colored by their VCS state, with the state shown in the tooltip.
- Single click opens the file in the editor.
- Context menu: Open, Open Containing Folder, Copy Full Path.
- Panel menu: show/hide untracked files (persisted in settings) plus the Git menu
  actions.
- Activation shortcut: `Meta+P` (macOS) / `Alt+P`.

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

## Support the development

If you find this project helpful, there are several ways you can support it:

- **Report issues** — if you encounter any bugs or have suggestions for
  improvements, please
  [open an issue](https://github.com/Palm1r/qt-creator-changes-panel/issues).
- **Contribute** — feel free to submit pull requests with bug fixes or new
  features.
- **Spread the word** — star the
  [GitHub repository](https://github.com/Palm1r/qt-creator-changes-panel) and
  share the plugin with your fellow developers.
- **Financial support** — if you'd like to support the development financially,
  you can make a donation using one of the following:
  - PayPal: [paypal.me/palm1r](https://www.paypal.com/paypalme/palm1r)
  - Bitcoin (BTC): `bc1qndq7f0mpnlya48vk7kugvyqj5w89xrg4wzg68t`
  - Ethereum (ETH): `0xA5e8c37c94b24e25F9f1f292a01AF55F03099D8D`
  - Litecoin (LTC): `ltc1qlrxnk30s2pcjchzx4qrxvdjt5gzuervy5mv0vy`
  - USDT (TRC20): `THdZrE7d6epW6ry98GA3MLXRjha1DjKtUx`

## For Contributors

### Code style

- C++: use the `.clang-format` configuration in the project root.
- Run formatting before submitting pull requests.

## Third-party assets

- `resources/icons/morevert*.png` — "more_vert" icon from
  [Material Design Icons](https://github.com/google/material-design-icons)
  by Google, licensed under the Apache License 2.0.

## License

Licensed under the [MIT License](LICENSE).

## Qt Creator components and attributions

Qt Creator Changes Panel is a plugin for Qt Creator and incorporates certain
components (plugin templates, API headers, and related boilerplate) originating
from Qt Creator, which are copyright (C) The Qt Company Ltd.

These components are provided by The Qt Company under the GNU General Public
License version 3, annotated with The Qt Company GPL Exception 1.0. This
exception permits the development and distribution of Qt Creator plugins under
licenses of the plugin author's own choosing, notwithstanding the GPL's general
linking requirements. It is this exception that allows Qt Creator Changes Panel
to be distributed under the MIT license.

The original copyright and license notices of The Qt Company are preserved in
the relevant source files and must not be removed.

# Installing the VIBR audio plugins

VIBR plugins come as **VST3**, **AU** (macOS only) and **Standalone** apps. The zips are named `VIBR-<group>-<format>-<arch>-<version>.zip`:

- `<group>`: `vintage`, `new`, `experimental`, `mastering`.
- `<format>`: `VST3`, `AU` or `Standalone`.
- `<arch>`: `arm64` (Apple silicon), `x86_64` (Intel Mac) or `win64` (Windows).

Pick the plugins that match your DAW: use the **AU** build in Logic Pro and GarageBand, and the **VST3** build in most other hosts (Ableton Live, Reaper, Cubase, Bitwig, Studio One, FL Studio ...).

## macOS (11 or newer, Apple silicon or Intel)

| Format | Copy to (current user) | Or for all users |
|---|---|---|
| VST3 | `~/Library/Audio/Plug-Ins/VST3/` | `/Library/Audio/Plug-Ins/VST3/` |
| AU | `~/Library/Audio/Plug-Ins/Components/` | `/Library/Audio/Plug-Ins/Components/` |
| Standalone | `/Applications/` (or anywhere) | |

1. Unzip the download. Use the build that matches your Mac (`arm64` for M1 and later, `x86_64` for Intel).
2. In Finder choose **Go > Go to Folder...** and paste the folder from the table (`~/Library` is hidden otherwise). Copy the `.vst3` / `.component` bundles into it.
3. If macOS says a plugin "cannot be opened because the developer cannot be verified", the file carries the download quarantine flag. Remove it in Terminal, then rescan in your DAW:

   ```sh
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/*.vst3
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/*.component
   ```

4. Logic Pro: after copying an AU, open **Logic Pro > Settings > Plug-in Manager** and press **Reset & Rescan Selection** if it does not show up. You can check an AU from Terminal with `auval -v aufx <code> <manufacturer>` (see `README.md`).

## Windows (10 or 11, 64-bit)

There is no AU on Windows. Use `VIBR-<group>-VST3-win64-<version>.zip` (and `Standalone` if you want the stand-alone apps).

| Format | Copy to |
|---|---|
| VST3 | `C:\Program Files\Common Files\VST3\` (all users, needs administrator rights) |
| Standalone | `C:\Program Files\VIBR\` (or anywhere) |

1. Unzip the download.
2. Copy the `.vst3` bundles (a folder per plugin) into `C:\Program Files\Common Files\VST3\`.
3. Restart your DAW and rescan its VST3 plugins.
4. If Windows SmartScreen blocks a standalone `.exe`, choose **More info > Run anyway**.

## Building it yourself

VIBR is open source. The code, the artwork and the build scripts are all free to read, modify and share under the GPL v3, and contributions are welcome: bug reports, ideas and pull requests. Get the source on GitHub: [github.com/RichardLeurs/VIBR-audio-plugins](https://github.com/RichardLeurs/VIBR-audio-plugins).

## Updating and uninstalling

- **Update:** copy the new bundles over the old ones (same folder, same name). Your presets and saved sessions keep working: parameter ids do not change between builds.
- **Uninstall:** delete the plugin bundles from the folders above. Plugin settings and MIDI mappings are stored inside your DAW project, not in separate files.

## Copyright and license

Copyright © 2026 Richard Leurs

VIBR audio plugins are free software: you can redistribute them and / or modify them under the terms of the **GNU General Public License, version 3**, as published by the Free Software Foundation. They are distributed in the hope that they will be useful, but **without any warranty**; without even the implied warranty of merchantability or fitness for a particular purpose.

The full license text is in [`LICENSE`](LICENSE). 

VIBR is built with the [JUCE](https://juce.com) framework (version 8), Copyright © Raw Material Software Limited. JUCE is dual-licensed under the [AGPLv3](https://www.gnu.org/licenses/agpl-3.0.en.html) and the commercial JUCE licence; VIBR uses it under the open-source terms. JUCE is a trademark of Raw Material Software Limited. See [github.com/juce-framework/JUCE](https://github.com/juce-framework/JUCE) for its license text.

VST is a registered trademark of Steinberg Media Technologies GmbH; Audio Units is a trademark of Apple Inc. All other names are used for identification only.

# Arctis Control

A native Linux control panel for **SteelSeries Arctis** headsets, built on top of
[HeadsetControl](https://github.com/Sapd/HeadsetControl).

*[Leer en español](README.es.md)*

> **Unofficial project**, not affiliated with SteelSeries. "SteelSeries" and "Arctis" are
> trademarks of their respective owners.

The app detects the connected headset and **only shows what that model supports**: battery,
equalizer (presets and bands with each model's range), auto power-off, volume limiter, lights,
voice prompts, Bluetooth, sidetone, microphone volume and mute LED, noise filter… If several
headsets are connected, you can pick which one to control.

Native C++/Qt 6 (QML) application: no browser, no server and no network connections.
The interface is in English and switches to Spanish automatically on Spanish systems.

## Supported headsets

Every SteelSeries model supported by HeadsetControl: Arctis 1, 7, 7+, 7X, 7P, 9, Pro (2019),
Pro Wireless, GameBuds, Nova 3, Nova 3P, Nova 5/5X, Nova 7, Nova 7P and Nova Pro Wireless.

Tested on an Arctis Nova 5. The other models have been tested with HeadsetControl's mock device
(`--test-device`), which exposes every feature. Reports from owners of other models are welcome.

## Installation (Arch Linux / CachyOS)

```
cd packaging
makepkg -si
```

This installs `/usr/bin/arctis-control`, the **Arctis Control** launcher and its icon. It replaces
the older `nova5-panel` package and imports its settings.

- Dependencies: `headsetcontrol`, `qt6-base`, `qt6-declarative`, `qt6-svg`.
- Build dependencies: `cmake`, `qt6-tools`.
- Optional: `ttf-fira-sans` for the interface font.

Uninstall with `sudo pacman -R arctis-control`.

If the headset does not show up, check that HeadsetControl sees it (`headsetcontrol -b`) and
replug the receiver after installing `headsetcontrol`, which adds the udev permission rules.

## Development

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/arctis-control
```

| Option | Purpose |
|---|---|
| `--test-device` | Use only HeadsetControl's mock headset. Real headsets and saved settings are never touched. |
| `--test-device --self-test` | Apply every setting to the mock through the same code as the UI; exits with 0 if all succeed. |
| `--screenshot file.png` | Save a screenshot and exit. With `QT_QPA_PLATFORM=offscreen` no window is shown. |

| Path | Contents |
|---|---|
| `src/` | `HeadsetController`: detects headsets, validates and runs `headsetcontrol`, stores settings |
| `qml/` | User interface: window, cards, setting controls, battery ring, equalizer bands |
| `i18n/` | Translations (source strings are in English) |
| `assets/` | Icon and headset illustration |
| `packaging/` | `PKGBUILD` and `.desktop` launcher |

### Translations

Source strings are in English. To add or update a language:

```
cmake --build build --target update_translations   # refresh i18n/*.ts from the sources
```

Then translate the `.ts` file with Qt Linguist (or any text editor) and rebuild; translations are
embedded in the binary. To add a new language, add its `i18n/arctis-control_<lang>.ts` to
`qt_add_translations` in `CMakeLists.txt`.

## Security

- No sockets and no network connections. The QML engine uses a network access manager that
  rejects every URL that is not an internal (qrc) resource.
- All text is rendered as plain text: messages containing HTML are never interpreted.
- `headsetcontrol` is run by absolute path with an argument list (no shell), a minimal
  environment, a 20 s timeout and a 1 MiB output cap; commands run one at a time.
- Every command carries `-d` with the selected headset, so it never reaches other connected ones.
- Values are validated against a closed table of settings and ranges and against the capabilities
  the headset reports. The result of each command is checked in HeadsetControl's response.
- Settings are stored per headset in `~/.config/arctis-control/state.json` (mode 0600, atomic
  writes); unknown keys are dropped and values are clamped when loading.
- The binary is built with PIE, full RELRO, `_FORTIFY_SOURCE`, stack protector, CET and no RUNPATH.
  `makepkg` uses the system `CFLAGS`; on CachyOS these may target the building machine's CPU, so
  build the package on the machine where it will run.

## Limitations

- HeadsetControl cannot read most settings back from the headset, so the panel shows the last
  value applied from it (except sidetone on models that report it).
- The parametric equalizer (Nova 3P, Nova 5) is not in the interface yet.
- ChatMix is only shown when the mix is off-centre.

## License

GPL-3.0-or-later (see `LICENSE`).

The icon uses the SteelSeries logo (Wikimedia Commons, `File:Steelseries_logo.svg`), recoloured;
it is a registered trademark and should be replaced with an original icon before the repository is
made public. The headset illustration (`assets/headset.svg`) is original artwork.

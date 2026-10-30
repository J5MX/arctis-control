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

If you use a separate microphone, you can also **turn the headset microphone off** so the system
(and apps such as Discord or games) no longer see it. This switches the headset's PipeWire sound
card to its output-only profile; WirePlumber remembers the choice.

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

- Dependencies: `headsetcontrol`, `qt6-base`, `qt6-declarative`, `qt6-svg`, `libpulse` (for
  `pactl`, used by the microphone switch with PipeWire).
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
| `src/` | `HeadsetController` (detects headsets, runs `headsetcontrol`, stores settings) and `HeadsetMic` (headset microphone on/off via `pactl`) |
| `qml/` | User interface: window, cards, setting controls, battery ring, equalizer bands |
| `i18n/` | Translations (source strings are in English) |
| `tests/security/` | Security test suite and fake `headsetcontrol` |
| `assets/` | Icon and headset illustration |
| `packaging/` | `PKGBUILD` and `.desktop` launcher |

### Tests

```
./build/arctis-control --test-device --self-test   # functional test against the mock headset
python3 tests/security/run.py                       # security tests (ASan + UBSan, a few minutes)
```

The security suite builds the app against a fake `headsetcontrol` and a fake `pactl` (CMake
options `ARCTIS_HEADSETCONTROL` and `ARCTIS_PACTL`, tests only) and feeds it hostile input:
malformed, huge, endless, hanging or crashing output, HTML in names and error messages, absurd
equalizer data, malicious `state.json` files and malicious PipeWire card data. Every case must exit cleanly with no sanitizer reports and no network request,
and every write command must target a single headset with `-d`.

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
- `headsetcontrol` and `pactl` are run by absolute path with an argument list (no shell), a
  minimal environment, a timeout and an output cap; commands run one at a time. Sound card and
  profile names read from PipeWire are validated before being passed back to `pactl`.
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

The app icon (`assets/icon.svg`) and the headset illustration (`assets/headset.svg`) are original
artwork, released under the same license.

# CLAUDE.md

Guidance for working on Arctis Control. The maintainer is new to development and GitHub:
explain steps plainly and talk to them in Spanish; everything in the repository is in English.

## What it is

Native C++/Qt 6 (QML) control panel for SteelSeries Arctis headsets on Linux, built on top of
[HeadsetControl](https://github.com/Sapd/HeadsetControl) (`/usr/bin/headsetcontrol`). It detects the
connected headset and only shows the features it reports. Extra pieces: turning the headset
microphone off through PipeWire (`pactl`), tray icon, low-battery notifications, start at login.

- Repo: https://github.com/J5MX/arctis-control (public). License GPL-3.0-or-later.
- Commit identity: `J5MX <35576890+J5MX@users.noreply.github.com>`. Commit messages in English.
- Tested hardware: Arctis Nova 5 (USB id `1038:2232`). Other models only via HeadsetControl's mock device.

## Layout

| Path | Contents |
|---|---|
| `src/headsetcontroller.*` | QML singleton. Runs `headsetcontrol -o json`, parses devices/capabilities, validates and applies settings, stores last applied values per headset in `~/.config/arctis-control/state.json` |
| `src/headsetmic.*` | QML singleton. Headset mic on/off by switching the PipeWire card profile with `pactl` |
| `src/appcontroller.*` | Tray icon, notifications (org.freedesktop.Notifications), autostart file, single instance via session D-Bus (`io.github.J5MX.ArctisControl`), app settings in `~/.config/arctis-control/settings.ini` |
| `src/batteryalerts.h` | Pure low-battery alert logic (unit tested) |
| `src/main.cpp` | Startup, translations, offline network factory, CLI options, `--self-test` |
| `qml/` | UI. `Main.qml` plus small components; `Theme.qml` is a singleton |
| `i18n/arctis-control_es.ts` | Spanish translation (source strings are English) |
| `tests/unit/` | QtTest unit tests (ctest) |
| `tests/security/` | Security suite with fake `headsetcontrol` and fake `pactl` |
| `tests/distros/run.sh` | Docker: builds, tests and installs on Arch, Fedora (COPR and stock 3.0), Debian 13, Ubuntu 26.04/24.04 |
| `INSTALL.md` | Tested install steps per distribution |
| `packaging/PKGBUILD` | Builds from the local checkout (`makepkg -si`) |
| `packaging/aur/` | AUR PKGBUILD (downloads the GitHub release tarball) and its `.SRCINFO` |

## Build and test

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
cmake --build build --target all_qmllint                   # must report no warnings
(cd build && ctest --output-on-failure)                    # unit tests
LANG=en_US.UTF-8 QT_QPA_PLATFORM=offscreen ./build/arctis-control --test-device --self-test
QT_QPA_PLATFORM=offscreen ./build/arctis-control --screenshot /path/shot.png   # UI check, no window
python3 tests/security/run.py                              # ASan/UBSan + hostile input, all must pass
cmake --build build --target update_translations           # refresh .ts after changing strings
```

QML-to-C++ coverage can be checked with a separate build configured with
`-DQT_QML_GENERATE_AOTSTATS=ON` and the `all_aotstats` target (currently ~98%).

## Rules that must not be broken

- **Every write command to headsetcontrol carries `-d 0xVVVV:0xPPPP`.** Without `-d` it applies to
  *every* connected headset, and with `--test-device` also to the real ones next to the mock. The
  `0x` prefixes are required. Never run manual `headsetcontrol` write commands against the user's
  real headset to "test" something; use `--test-device -d 0xf00b:0xa00c`.
- Do not change the user's real PipeWire card profile in tests (it could change the default
  microphone). Use the fake `pactl`.
- Never use a desktop screenshot tool (it captures whatever window is active, e.g. private chats).
  Use `--screenshot` with `QT_QPA_PLATFORM=offscreen`.
- Development modes (`--test-device`, `--screenshot`) never save settings, never claim the D-Bus
  name and never show a tray icon.
- Every QML `Text` sets `textFormat: Text.PlainText` (AutoText renders HTML and loads remote images).
- External programs: absolute path, argument list (no shell), minimal environment, timeout and
  output cap. Values are validated against the setting table and the reported capabilities.
- No network access: the QML engine's network manager only allows `qrc:`.
- Do not attach prebuilt binaries to releases: makepkg here uses `-march=native` (AVX-512).

## Gotchas already hit

- `QJsonObject` sorts keys, but `-p N` uses the original preset order: it is read from the raw text.
- `QJsonValue::toInt()` returns 0 for numbers that do not fit an int; use `jsonInt()` to clamp.
  Battery level `-1` means "off/unknown" and must stay `-1`.
- Results of write commands come in the `actions` array of the JSON; no actions means it failed.
- Basic-style `Switch` without text gets width 0: `Toggle.qml` sets its implicit size.
- `ItemDelegate.text` is FINAL: model roles cannot be called `text` (use `label`).
- QtSvg does not support nested `<svg>` or `textPath`.
- Member destruction order: the `QProcess` destructor emits `finished()`; destructors disconnect
  and stop the process first.
- `QStringLiteral` strings are UTF-16 in the binary (`strings -el` to find them).
- Minimum Qt is 6.8 (Debian 13): do not use newer QML syntax such as `final` properties.
- HeadsetControl 3.x has no `-d`: the app probes `headsetcontrol -h` for `--device` and then only
  writes when a single device is listed. It may live in `/usr/bin` or `/usr/local/bin`.
- Switching the PipeWire card profile (mic on/off) recreates the sink: browsers pause playback.
- Wayland never reports the minimized state to apps (verified with KWin): no minimize-to-tray there.
- Shell is zsh: quote URLs with `?`, `$c:a` is a modifier (use `${c}`), unquoted `$var` is not split.

## Releasing a version

1. Bump the version in `CMakeLists.txt` (`project(... VERSION x.y.z)`) and `packaging/PKGBUILD`.
2. Add the entry to `CHANGELOG.md`; update `README.md` / `README.es.md` if features changed.
3. Run lint, ctest, self-test and the security suite; build the package with `makepkg`.
4. Commit, tag `vX.Y.Z` (annotated), the maintainer pushes (`git push --follow-tags`).
5. Publish the GitHub release from the tag (pre-release before 1.0), notes taken from the changelog.
6. AUR: update `pkgver` and `sha256sums` in `packaging/aur/PKGBUILD` (checksum of
   `https://github.com/J5MX/arctis-control/archive/refs/tags/vX.Y.Z.tar.gz`), regenerate `.SRCINFO`
   with `makepkg --printsrcinfo`, test with `makepkg` in an empty folder, then commit and push in the
   separate AUR repository (`ssh://aur@aur.archlinux.org/arctis-control.git`, branch `master`).

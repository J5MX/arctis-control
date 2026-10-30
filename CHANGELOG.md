# Changelog

All notable changes to Arctis Control. Versions before 1.0 are previews.

## 0.5.0 — 2026-10-30

### Added
- "Use the headset microphone" switch: turns the headset microphone off for people who use a
  separate one, by switching its PipeWire sound card to the matching output-only profile (and
  back). Works for any headset with a PipeWire card, independently of HeadsetControl.
- Self-test and security suite cover it with a fake `pactl` (real Nova 5 profiles, hostile card
  data, failures, hangs).

## 0.4.2 — 2026-10-30

### Changed
- New original app icon (headset with equalizer bars), replacing the recoloured SteelSeries logo.

## 0.4.1 — 2026-10-30

### Fixed
- Use-after-destroy when closing the app while `headsetcontrol` was still running.
- Settings could be saved to the wrong headset if the selection changed while a command ran.
- The QML network blocker was destroyed before the engine that used it.
- Auto power-off kept showing a dash after choosing "Never".
- Out-of-range numbers in JSON became 0 instead of being clamped.
- "Play notification sound" failed silently on headsets without that feature.

### Changed
- Limits on everything read from `headsetcontrol` (devices, names, capabilities, presets,
  equalizer geometry).
- `state.json` may be a symlink to a regular file (dotfile managers); FIFOs and directories
  are still rejected.
- The UI is only refreshed when the headset status actually changes; presets are parsed once
  per device. 98% of the QML bindings compile to C++.

### Added
- Security test suite (`tests/security/run.py`): ASan/UBSan build against a fake
  `headsetcontrol` with 26 hostile-input cases.

## 0.4.0 — 2026-10-30

### Added
- English user interface with automatic Spanish translation (Qt Linguist).
- `README.es.md` (Spanish README).

### Changed
- Code comments, README, PKGBUILD and desktop entry are now in English.
- Numbers use the locale's decimal separator.

## 0.3.0 — 2026-10-30

### Added
- Support for every SteelSeries headset handled by HeadsetControl. The UI only shows the
  features the connected model reports (battery, equalizer with each model's range, presets,
  auto power-off, volume limiter, lights, voice prompts, Bluetooth, sidetone, microphone
  volume, mute LED, noise filter, rotate to mute).
- Headset picker when several are connected; settings are stored per headset.
- `--test-device` (mock headset only) and `--self-test`.

### Changed
- Renamed from `nova5-panel` to `arctis-control`; the package replaces `nova5-panel` and
  imports its settings.
- Every command targets the selected headset with `-d`; results are checked per action.
- License: GPL-3.0-or-later.

### Removed
- The previous web version (local Python server + browser).

## 0.2.1 — 2026-10-30

### Security
- All text rendered as plain text (an HTML `<img>` in an error message triggered a network
  request); QML network access limited to internal resources; `headsetcontrol` output capped
  at 1 MiB; no RUNPATH.

## 0.2.0 — 2026-10-30

- First native C++/Qt 6 version for the Arctis Nova 5, packaged with a PKGBUILD.

## 0.1.0 — 2026-10-30

- First version: local web panel for the Arctis Nova 5.

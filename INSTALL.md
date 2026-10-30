# Installing Arctis Control

Every set of commands below was tested on a clean system (Docker containers, see
`tests/distros/run.sh`): it builds the app, runs its tests and installs it.

**Requirements:** Qt 6.8 or newer, [HeadsetControl](https://github.com/Sapd/HeadsetControl)
(4.0 or newer recommended; 3.x works with a single headset connected) and `pactl` (PipeWire or
PulseAudio) for the microphone switch.

| Distribution | Status |
|---|---|
| Arch Linux, CachyOS and other Arch-based | ✅ Supported (package with `makepkg`) |
| Fedora | ✅ Supported (HeadsetControl from its COPR) |
| Ubuntu 26.04 LTS | ✅ Supported (HeadsetControl from its PPA) |
| Debian 13 "trixie" | ✅ Supported (HeadsetControl built from source) |
| Ubuntu 24.04 LTS and older, Debian 12 | ❌ Qt is too old (6.4 or earlier) |

After installing, open **Arctis Control** from your application menu (or run `arctis-control`).
If the headset does not show up, replug its USB receiver so the HeadsetControl permission rules
apply, and check that `headsetcontrol -b` sees it.

## Arch Linux / CachyOS

```
sudo pacman -S --needed git base-devel
git clone https://github.com/J5MX/arctis-control.git
cd arctis-control/packaging
makepkg -si
```

Run `makepkg` **without** `sudo`. Update: `git pull` in `arctis-control` and `makepkg -si` again
in `packaging`. Uninstall: `sudo pacman -R arctis-control`.

## Fedora

```
sudo dnf install git cmake gcc-c++ qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtsvg-devel \
    qt6-qttools-devel qt6-linguist pulseaudio-utils dnf-plugins-core
sudo dnf copr enable thesapd/headsetcontrol
sudo dnf install headsetcontrol
```

Fedora's own `headsetcontrol` package is version 3.0: it works, but it cannot choose between
several connected headsets, so the COPR (HeadsetControl's official repository) is recommended.

Then [build and install the app](#build-and-install-the-app).

## Ubuntu 26.04 LTS

```
sudo apt install git cmake g++ make qt6-base-dev qt6-declarative-dev qt6-svg-dev qt6-tools-dev \
    qt6-tools-dev-tools qt6-l10n-tools libgl-dev qml6-module-qtquick qml6-module-qtquick-controls \
    qml6-module-qtquick-layouts qml6-module-qtquick-shapes qml6-module-qtquick-templates \
    qml6-module-qtquick-window qml6-module-qtqml-workerscript qt6-qpa-plugins pulseaudio-utils \
    software-properties-common
sudo add-apt-repository ppa:sapd/headsetcontrol
sudo apt install headsetcontrol
```

Then [build and install the app](#build-and-install-the-app). Ubuntu 25.10 has a recent enough Qt,
but the PPA has no packages for it: build HeadsetControl from source as on Debian.

## Debian 13 "trixie"

HeadsetControl is not packaged in Debian, so it is built from source first:

```
sudo apt install git cmake g++ make qt6-base-dev qt6-declarative-dev qt6-svg-dev qt6-tools-dev \
    qt6-tools-dev-tools qt6-l10n-tools libgl-dev qml6-module-qtquick qml6-module-qtquick-controls \
    qml6-module-qtquick-layouts qml6-module-qtquick-shapes qml6-module-qtquick-templates \
    qml6-module-qtquick-window qml6-module-qtqml-workerscript qt6-qpa-plugins pulseaudio-utils \
    libhidapi-dev
git clone --depth 1 -b 4.2.0 https://github.com/Sapd/HeadsetControl.git
cmake -S HeadsetControl -B HeadsetControl/build -DCMAKE_BUILD_TYPE=Release
cmake --build HeadsetControl/build
sudo cmake --install HeadsetControl/build
sudo udevadm control --reload-rules && sudo udevadm trigger
```

This installs `headsetcontrol` in `/usr/local/bin` (Arctis Control finds it there) and its
permission rules in `/usr/local/lib/udev/rules.d`; the last command applies them without
rebooting. Then [build and install the app](#build-and-install-the-app).

## Build and install the app

For Fedora, Ubuntu and Debian, once the dependencies above are installed:

```
git clone https://github.com/J5MX/arctis-control.git
cd arctis-control
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

The app goes to `/usr/local` (`/usr/local/bin/arctis-control`, its launcher and icon), away from
the files managed by your package manager.

- **Update:** in `arctis-control`, run `git pull`, then the last two commands again.
- **Uninstall:** in `arctis-control`, run `sudo xargs rm < build/install_manifest.txt`.
- Optional: the interface is designed for the Fira Sans Condensed font (on Arch, package
  `ttf-fira-sans`; Fedora, Debian and Ubuntu do not package it). Without it, a similar system
  font is used and everything works the same.

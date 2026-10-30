#!/bin/bash
# Builds and tests Arctis Control on other distributions inside Docker containers.
# Uses the local checkout (mounted read-only); nothing is installed on the host.
#
# Usage: tests/distros/run.sh [distro ...]     (default: all)
#        distros: arch fedora fedora-stock debian ubuntu (26.04) ubuntu-lts (24.04)
#
# HeadsetControl is installed the way the README recommends for each distro:
# Fedora from its COPR (fedora-stock uses Fedora's own, older 3.x package),
# Ubuntu from its PPA, Debian built from source (it is not packaged there).
set -u
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
LOGS=${LOGS:-$ROOT/build/distro-logs}
mkdir -p "$LOGS"

image() {
    case $1 in
        arch) echo archlinux:latest ;;
        fedora|fedora-stock) echo fedora:latest ;;
        debian) echo debian:trixie ;;
        ubuntu) echo ubuntu:26.04 ;;
        ubuntu-lts) echo ubuntu:24.04 ;;
    esac
}

FEDORA_DEPS="dnf -y install cmake gcc-c++ qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtsvg-devel qt6-qttools-devel qt6-linguist pulseaudio-utils"
APT_DEPS="apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends cmake g++ make qt6-base-dev qt6-declarative-dev qt6-svg-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools libgl-dev qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts qml6-module-qtquick-shapes qml6-module-qtquick-templates qml6-module-qtquick-window qml6-module-qtqml-workerscript qt6-qpa-plugins pulseaudio-utils"

install_cmd() {
    case $1 in
        arch) echo "pacman -Syu --noconfirm --needed base-devel cmake qt6-base qt6-declarative qt6-svg qt6-tools libpulse headsetcontrol" ;;
        fedora) echo "$FEDORA_DEPS && dnf -y install dnf-plugins-core && dnf -y copr enable thesapd/headsetcontrol && dnf -y install headsetcontrol" ;;
        fedora-stock) echo "$FEDORA_DEPS && dnf -y install headsetcontrol" ;;
        debian) echo "$APT_DEPS git ca-certificates libhidapi-dev && git clone -q --depth 1 -b 4.2.0 https://github.com/Sapd/HeadsetControl.git /tmp/hc && cmake -S /tmp/hc -B /tmp/hc/b -DCMAKE_BUILD_TYPE=Release && cmake --build /tmp/hc/b -j8 && cmake --install /tmp/hc/b" ;;
        ubuntu|ubuntu-lts) echo "$APT_DEPS software-properties-common gpg-agent && add-apt-repository -y ppa:sapd/headsetcontrol && apt-get install -y headsetcontrol" ;;
    esac
}

# Runs inside the container.
read -r -d '' STEPS <<'IN'
set -e
echo "### qt: $(qmake6 -query QT_VERSION 2>/dev/null || qtpaths6 --qt-version 2>/dev/null || echo ?)"
echo "### headsetcontrol: $(headsetcontrol --version 2>&1 | head -1) | supports -d: $(headsetcontrol -h 2>&1 | grep -q -- --device && echo yes || echo no)"
cp -r /src /tmp/src && rm -rf /tmp/src/build /tmp/src/packaging/src /tmp/src/packaging/pkg
cmake -S /tmp/src -B /tmp/b -DCMAKE_BUILD_TYPE=Release > /tmp/cmake.log 2>&1 || { tail -20 /tmp/cmake.log; exit 10; }
cmake --build /tmp/b -j"$(nproc)" > /tmp/build.log 2>&1 || { grep -m15 -E "error|Error" /tmp/build.log; exit 11; }
echo "### build: ok"
(cd /tmp/b && ctest --output-on-failure) > /tmp/ctest.log 2>&1 && echo "### unit tests: ok" || { cat /tmp/ctest.log; exit 12; }
set +e
LANG=C.UTF-8 QT_QPA_PLATFORM=offscreen timeout 120 /tmp/b/arctis-control --test-device --self-test > /tmp/self.log 2>&1
rc=$?; cat /tmp/self.log
echo "### self-test exit: $rc ($(grep -c '^OK' /tmp/self.log) ok, $(grep -c '^FAIL' /tmp/self.log) failed, $(grep -c '^SKIP' /tmp/self.log) skipped)"
[ $rc = 0 ] || exit $rc
# Install as the README says and check the installed app.
cmake --install /tmp/b > /tmp/install-app.log 2>&1 || { tail /tmp/install-app.log; exit 13; }
echo "### installed: $(command -v arctis-control) $(arctis-control --version 2>&1 | tail -1) | launcher: $(ls /usr/local/share/applications/arctis-control.desktop /usr/share/applications/arctis-control.desktop 2>/dev/null | head -1)"
echo "### headsetcontrol udev rules: $(ls /usr/lib/udev/rules.d/*headset* /etc/udev/rules.d/*headset* /usr/local/lib/udev/rules.d/*headset* 2>/dev/null | tr '\n' ' ' || true)"
exit 0
IN

for distro in "${@:-arch fedora fedora-stock debian ubuntu ubuntu-lts}"; do
    for d in $distro; do
        log="$LOGS/$d.log"
        echo "== $d ($(image "$d"))"
        docker run --rm -v "$ROOT:/src:ro" "$(image "$d")" bash -c "$(install_cmd "$d") > /tmp/install.log 2>&1 || { tail -15 /tmp/install.log; exit 9; }; $STEPS" > "$log" 2>&1
        rc=$?
        grep -E "^### " "$log"
        echo "   result: $([ $rc = 0 ] && echo PASS || echo "FAIL ($rc), see $log")"
    done
done

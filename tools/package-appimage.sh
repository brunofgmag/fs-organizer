#!/usr/bin/env bash
set -euo pipefail

usage="usage: package-appimage.sh <build-dir> <output-dir> [version]"
build_dir="$(realpath "${1:?$usage}")"
output_dir="$(realpath -m "${2:?$usage}")"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
version="${3:-$(tr -d '[:space:]' < "$root/VERSION.txt")}"
qt_root="${QT_ROOT_DIR:?QT_ROOT_DIR must name the Qt kit the build used}"

linuxdeploy_url="https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20250213-2/linuxdeploy-x86_64.AppImage"
linuxdeploy_sha="4648f278ab3ef31f819e67c30d50f462640e5365a77637d7e6f2ad9fd0b4522a"
qt_plugin_url="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage"
qt_plugin_sha="15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724"

tools_dir="${FSORG_TOOLS_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/fs-organizer/linuxdeploy}"
work_dir="$(mktemp -d)"
trap 'rm -rf "$work_dir"' EXIT

fetch() {
  local url="$1" sha="$2" file="$tools_dir/$(basename "$1")"
  mkdir -p "$tools_dir"
  if ! echo "$sha  $file" | sha256sum --check --status 2>/dev/null; then
    curl --fail --silent --show-error --location --output "$file" "$url"
    echo "$sha  $file" | sha256sum --check --status
  fi
  chmod +x "$file"
}

library_path() {
  ldconfig -p | awk -v name="$1" '$1 == name && !found { print $NF; found = 1 }'
}

fetch "$linuxdeploy_url" "$linuxdeploy_sha"
fetch "$qt_plugin_url" "$qt_plugin_sha"

app_dir="$work_dir/AppDir"
DESTDIR="$app_dir" cmake --install "$build_dir" --prefix /usr

ssl_libraries=()
for name in libssl.so.3 libcrypto.so.3; do
  path="$(library_path "$name")"
  if [ -n "$path" ]; then
    ssl_libraries+=(--library "$path")
  fi
done

export APPIMAGE_EXTRACT_AND_RUN=1
export PATH="$tools_dir:$PATH"
export QMAKE="$qt_root/bin/qmake"
export LD_LIBRARY_PATH="$qt_root/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export EXTRA_PLATFORM_PLUGINS="libqwayland-generic.so;libqwayland-egl.so;libqoffscreen.so"
export LINUXDEPLOY_OUTPUT_VERSION="$version"
export LINUXDEPLOY_OUTPUT_APP_NAME="fs-organizer"

linuxdeploy="$tools_dir/$(basename "$linuxdeploy_url")"
plugins="$app_dir/usr/plugins"

mkdir -p "$plugins" "$output_dir"

for kind in wayland-shell-integration wayland-graphics-integration-client wayland-decoration-client; do
  cp -r "$qt_root/plugins/$kind" "$plugins/"
done
mkdir -p "$plugins/platformthemes"
cp "$qt_root/plugins/platformthemes/libqxdgdesktopportal.so" "$plugins/platformthemes/"

(
  cd "$work_dir"
  "$linuxdeploy" \
    --appdir "$app_dir" \
    --executable "$app_dir/usr/bin/fs-organizer" \
    --desktop-file "$app_dir/usr/share/applications/fs-organizer.desktop" \
    --icon-file "$root/assets/branding/icon_256.png" \
    --icon-filename fs-organizer \
    --deploy-deps-only "$plugins" \
    "${ssl_libraries[@]}" \
    --plugin qt
)

install -m 644 "$root/assets/linux/apprun-hook.sh" "$app_dir/apprun-hooks/linuxdeploy-plugin-qt-hook.sh"

translations="$app_dir/usr/translations"
find "$translations" -type f -delete
for catalogue in qt_en qt_pt_BR qtbase_pt_BR; do
  install -m 644 "$qt_root/translations/$catalogue.qm" "$translations/"
done

(
  cd "$work_dir"
  "$linuxdeploy" --appdir "$app_dir" --output appimage
)

image="$output_dir/fs-organizer-$version-x86_64.AppImage"
mv "$work_dir"/fs-organizer-*-x86_64.AppImage "$image"
chmod +x "$image"
printf '%s  %s' "$(sha256sum "$image" | cut -d' ' -f1)" "$(basename "$image")" > "$image.sha256"

echo "$image"

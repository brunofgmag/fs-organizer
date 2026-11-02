#!/usr/bin/env bash
set -euo pipefail

usage="usage: build-libarchive.sh <prefix>"
prefix="$(realpath -m "${1:?$usage}")"

version="3.8.11"
sha="a3dc0872b1b39da34dc18ef1b944f0f11ffd37425d70ae04be679279f1cf43d2"
url="https://github.com/libarchive/libarchive/releases/download/v$version/libarchive-$version.tar.xz"

work_dir="$(mktemp -d)"
trap 'rm -rf "$work_dir"' EXIT

file="$work_dir/$(basename "$url")"
curl --fail --silent --show-error --location --output "$file" "$url"
echo "$sha  $file" | sha256sum --check --status

tar -xf "$file" -C "$work_dir"
cd "$work_dir/libarchive-$version"

./configure \
  --prefix="$prefix" \
  --disable-static \
  --disable-bsdtar \
  --disable-bsdcat \
  --disable-bsdcpio \
  --disable-bsdunzip \
  --disable-acl \
  --disable-xattr \
  --without-xml2 \
  --without-expat \
  --without-nettle

make --jobs="$(nproc)"
make install

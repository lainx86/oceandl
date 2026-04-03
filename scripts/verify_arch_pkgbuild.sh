#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "usage: $0 <source-release.tar.gz> <pkgbuild-dir>" >&2
  exit 2
fi

archive_path=$(realpath "$1")
pkgbuild_dir=$(realpath "$2")
archive_sha256=$(sha256sum "$archive_path" | awk '{print $1}')

work_dir=$(mktemp -d)
cleanup() {
  rm -rf "$work_dir"
}
trap cleanup EXIT

archive_copy="$work_dir/$(basename "$archive_path")"
cp -a "$archive_path" "$archive_copy"
cp -a "$pkgbuild_dir"/. "$work_dir/pkgbuild"

if [ "$EUID" -eq 0 ]; then
  builder_user=oceandl-builder
  if ! id -u "$builder_user" >/dev/null 2>&1; then
    useradd --create-home --shell /bin/bash "$builder_user"
  fi
  chown -R "$builder_user:$builder_user" "$work_dir"
  su -s /bin/bash "$builder_user" -c "
    cd \"$work_dir/pkgbuild\" && \
    OCEANDL_SOURCE_URL=\"file://${archive_copy}\" \
    OCEANDL_SOURCE_SHA256=\"$archive_sha256\" \
    makepkg --force --nodeps --cleanbuild
  "
else
  pushd "$work_dir/pkgbuild" >/dev/null
  OCEANDL_SOURCE_URL="file://${archive_copy}" \
  OCEANDL_SOURCE_SHA256="$archive_sha256" \
  makepkg --force --nodeps --cleanbuild
  popd >/dev/null
fi

package_path=$(
  find "$work_dir/pkgbuild" -maxdepth 1 -type f \
    -name 'oceandl-*.pkg.tar*' \
    ! -name 'oceandl-debug-*.pkg.tar*' \
    | sort \
    | head -n 1
)
if [ -z "$package_path" ]; then
  echo "makepkg did not produce a main oceandl package archive" >&2
  exit 1
fi

extract_root="$work_dir/extract"
mkdir -p "$extract_root"
bsdtar -xf "$package_path" -C "$extract_root"

binary_path="$extract_root/usr/bin/oceandl"
if [ ! -x "$binary_path" ]; then
  echo "expected packaged binary not found at $binary_path" >&2
  exit 1
fi

"$binary_path" --version
"$binary_path" datasets >/dev/null
"$binary_path" providers >/dev/null
"$binary_path" download --help >/dev/null

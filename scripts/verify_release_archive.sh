#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "usage: $0 <archive.tar.gz>" >&2
  exit 2
fi

archive_path=$1
extract_root=$(mktemp -d)
cleanup() {
  rm -rf "$extract_root"
}
trap cleanup EXIT

case "$archive_path" in
  *.tar.gz|*.tgz)
    tar -C "$extract_root" -xzf "$archive_path"
    ;;
  *)
    echo "unsupported archive type: $archive_path" >&2
    exit 2
    ;;
esac

package_dir=$(find "$extract_root" -mindepth 1 -maxdepth 1 -type d | head -n 1)
if [ -z "$package_dir" ]; then
  echo "archive did not extract a top-level directory" >&2
  exit 1
fi

binary_path="$package_dir/bin/oceandl"
if [ ! -x "$binary_path" ]; then
  echo "expected executable not found at $binary_path" >&2
  exit 1
fi

"$binary_path" --version
"$binary_path" datasets >/dev/null
"$binary_path" providers >/dev/null
"$binary_path" download --help >/dev/null

case "$(uname -s)" in
  Linux)
    ldd "$binary_path"
    ldd "$binary_path" | grep -F "$package_dir/bin/libcurl" >/dev/null
    ldd "$binary_path" | grep -F "$package_dir/bin/libfmt" >/dev/null
    ldd "$binary_path" | grep -F "$package_dir/bin/libtomlplusplus" >/dev/null
    ;;
  Darwin)
    otool -L "$binary_path"
    otool -L "$binary_path" | grep '@executable_path/../Frameworks/' >/dev/null
    ;;
  *)
    echo "unsupported platform for release archive verification: $(uname -s)" >&2
    exit 2
    ;;
esac

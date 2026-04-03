#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "usage: $0 <version> <output.tar.gz>" >&2
  exit 2
fi

version=$1
output_path=$2

case "$version" in
  v*)
    echo "pass the plain version without a leading v: $version" >&2
    exit 2
    ;;
esac

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
output_dir=$(dirname "$output_path")
output_name=$(basename "$output_path")
prefix="oceandl-v${version}"

mkdir -p "$output_dir"
tmp_output="$output_dir/.${output_name}.tmp"

(
  cd "$repo_root"
  while IFS= read -r -d '' path; do
    if [ -e "$path" ]; then
      printf '%s\0' "$path"
    fi
  done < <(git -c safe.directory="$repo_root" ls-files -z) \
    | tar \
      --null \
      --files-from=- \
      --sort=name \
      --mtime='UTC 1970-01-01' \
      --owner=0 \
      --group=0 \
      --numeric-owner \
      --transform "s,^,${prefix}/," \
      -cf -
) | gzip -n > "$tmp_output"

mv "$tmp_output" "$output_path"

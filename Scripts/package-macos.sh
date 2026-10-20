#!/bin/bash
# Build distribution artifacts from a prebuilt universal Release Audio Unit.
# No Developer ID signature or Apple notarization is applied here.
set -euo pipefail

version="0.7.0"
build_dir="build"
output_dir="dist"
if (( $# >= 1 )); then version="$1"; fi
if (( $# >= 2 )); then build_dir="$2"; fi
if (( $# >= 3 )); then output_dir="$3"; fi

component="$build_dir/ViFilter_artefacts/Release/AU/Vi Filter.component"
binary="$component/Contents/MacOS/Vi Filter"

if [[ ! -f "$binary" ]]; then
  echo "Missing Release AU: $binary" >&2
  exit 1
fi

lipo "$binary" -verify_arch arm64 x86_64 || {
  echo "AU is not universal (arm64 + x86_64)." >&2
  exit 1
}

mkdir -p "$output_dir"
output_dir="$(cd "$output_dir" && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
install_root="$work/root"
mkdir -p "$install_root/Library/Audio/Plug-Ins/Components"
ditto "$component" "$install_root/Library/Audio/Plug-Ins/Components/Vi Filter.component"

zipfile="$output_dir/ViFilter-$version-macOS-universal.zip"
pkgfile="$output_dir/ViFilter-$version-macOS-universal-unsigned.pkg"

# ZIP installs manually into ~/Library/Audio/Plug-Ins/Components/.
ditto -c -k --sequesterRsrc --keepParent "$component" "$zipfile"

# PKG installs system-wide under /Library/Audio/Plug-Ins/Components/.
# Unsigned .pkg is for local testing ONLY; sign/notarize for distribution.
pkgbuild --root "$install_root" \
  --identifier "com.markxxv.vifilter.pkg" \
  --version "$version" \
  --install-location / \
  "$pkgfile"

(cd "$output_dir" && shasum -a 256 "$(basename "$zipfile")" "$(basename "$pkgfile")" > SHA256SUMS.txt)
echo "Artifacts:"
ls -lh "$zipfile" "$pkgfile" "$output_dir/SHA256SUMS.txt"

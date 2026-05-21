#!/usr/bin/env bash
#
# Assemble an Arduino-shaped copy of json-voorhees under build/arduino-library/JsonVoorhees
# so arduino-cli can compile it via arduino/compile-sketches. The library's source layout
# (include/jsonv + src/jsonv) is preserved by projecting them under src/jsonv and src/impl
# in the output, leaving the upstream tree untouched.
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
out_root=${1:-"${repo_root}/build/arduino-library"}
lib_root="${out_root}/JsonVoorhees"

rm -rf "${lib_root}"
mkdir -p "${lib_root}/src" "${lib_root}/examples"

cp "${repo_root}/packaging/arduino/library.properties" "${lib_root}/library.properties"
cp -R "${repo_root}/packaging/arduino/examples/." "${lib_root}/examples/"

cp -R "${repo_root}/include/jsonv" "${lib_root}/src/jsonv"
cp -R "${repo_root}/src/jsonv"     "${lib_root}/src/impl"

echo "${lib_root}"

#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root_dir"

cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
cmake --install .build/release --prefix .build/package-root
cpack --config .build/release/CPackConfig.cmake -B .build/packages

printf '安装目录: %s\n' "$root_dir/.build/package-root"
printf '发布包目录: %s\n' "$root_dir/.build/packages"


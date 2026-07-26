$ErrorActionPreference = "Stop"
$build = Join-Path $PSScriptRoot "..\build"
cmake -S (Join-Path $PSScriptRoot "..") -B $build
cmake --build $build
ctest --test-dir $build --output-on-failure

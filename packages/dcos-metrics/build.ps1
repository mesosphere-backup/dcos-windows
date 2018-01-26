# includes basic code from building

[CmdletBinding(DefaultParameterSetName="Standard")]
param(
    [string]
    [ValidateNotNullOrEmpty()]
    $pkgSrc,  # Location of the packages tree sources

    [string]
    [ValidateNotNullOrEmpty()]
    $pkgDest  # Location of the packages tree compiled binaries

)

$METRICS_DIR = Join-Path $env:TEMP "metrics-src"

Write-Host "Building project"
copy-item -Recurse "$pkgSrc" -destination "$METRICS_DIR"
Push-Location $METRICS_DIR
. .\scripts\build.ps1 collector stdsd-emitter plugins
Copy-Item -Path "$METRICS_DIR\build\collector\" -Destination "$pkgDest" -filter "*.exe"
Copy-Item -Path "$METRICS_DIR\build\plugins\" -Destination "$pkgDest" -filter "*.exe"
Copy-Item -Path "$METRICS_DIR\build\statsd-emitter\" -Destination "$pkgDest" -filter "*.exe"
Pop-Location

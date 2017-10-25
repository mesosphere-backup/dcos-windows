../../builds.ps1  # includes basic code from building

[CmdletBinding(DefaultParameterSetName="Standard")]
param(
    [string]
    [ValidateNotNullOrEmpty()]
    $pkgSrc  # Location of the packages tree sources

    [string]
    [ValidateNotNullOrEmpty()]
    $pkgDest  # Location of the packages tree compiled binaries

)

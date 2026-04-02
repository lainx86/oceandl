param(
    [Parameter(Mandatory = $true)]
    [string]$ArchivePath
)

$ErrorActionPreference = "Stop"

$resolvedArchivePath = (Resolve-Path $ArchivePath).Path
$extractRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("oceandl-release-smoke-" + [guid]::NewGuid())

try {
    New-Item -ItemType Directory -Path $extractRoot | Out-Null
    Expand-Archive -Path $resolvedArchivePath -DestinationPath $extractRoot -Force

    $packageDir = Get-ChildItem $extractRoot | Where-Object { $_.PSIsContainer } | Select-Object -First 1
    if (-not $packageDir) {
        throw "Archive did not extract a top-level directory."
    }

    $binaryPath = Join-Path $packageDir.FullName "bin/oceandl.exe"
    if (-not (Test-Path $binaryPath)) {
        throw "Expected executable not found at $binaryPath."
    }

    & $binaryPath --version
    & $binaryPath datasets | Out-Null
    & $binaryPath providers | Out-Null
    & $binaryPath download --help | Out-Null

    $bundledDlls = Get-ChildItem (Join-Path $packageDir.FullName "bin") -Filter *.dll -ErrorAction SilentlyContinue
    if (-not $bundledDlls) {
        throw "No bundled runtime DLLs were found under $($packageDir.FullName)\bin."
    }
}
finally {
    if (Test-Path $extractRoot) {
        Remove-Item -Recurse -Force $extractRoot
    }
}

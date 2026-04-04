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

    foreach ($docName in @("README.md", "LICENSE", "CHANGELOG.md")) {
        $docPath = Join-Path $packageDir.FullName ("share/doc/oceandl/" + $docName)
        if (-not (Test-Path $docPath)) {
            throw "Expected bundled document not found at $docPath."
        }
    }

    & $binaryPath --version
    & $binaryPath datasets | Out-Null
    & $binaryPath providers | Out-Null
    & $binaryPath download --help | Out-Null

    $bundledDlls = Get-ChildItem (Join-Path $packageDir.FullName "bin") -Filter *.dll -ErrorAction SilentlyContinue
    if ($bundledDlls) {
        $dllNames = ($bundledDlls | Select-Object -ExpandProperty Name) -join ", "
        throw "The maintained Windows release path is expected to be self-contained without extra runtime DLLs, but found: $dllNames"
    }
}
finally {
    if (Test-Path $extractRoot) {
        Remove-Item -Recurse -Force $extractRoot
    }
}

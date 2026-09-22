# Verifies the staged Windows application before NSIS packages it.
# The release must carry every non-Windows DLL it imports, and the staged app
# must pass the same runtime check that Setup runs after extraction.
param(
    [Parameter(Mandatory = $true)][string] $Dist,
    [string] $ObjDump = 'H:\Qt\Tools\mingw1310_64\bin\objdump.exe'
)
$ErrorActionPreference = 'Stop'

$Dist = (Resolve-Path -LiteralPath $Dist).Path
if (-not (Test-Path -LiteralPath $ObjDump -PathType Leaf)) {
    throw "objdump.exe not found: $ObjDump"
}

$required = @(
    'QualityOfLifeModManager.exe',
    'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'Qt6Network.dll', 'Qt6Svg.dll',
    'libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll',
    'D3Dcompiler_47.dll',
    'platforms\qwindows.dll', 'styles\qmodernwindowsstyle.dll',
    'tls\qschannelbackend.dll', 'imageformats\qjpeg.dll'
)
foreach ($relative in $required) {
    $path = Join-Path $Dist $relative
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "runtime file missing from dist: $relative"
    }
}

$files = @(Get-ChildItem -LiteralPath $Dist -Recurse -File |
    Where-Object { $_.Extension -in '.exe', '.dll' })
$bundled = @{}
foreach ($file in $files) {
    $bundled[$file.Name.ToLowerInvariant()] = $true
}

$problems = [System.Collections.Generic.List[string]]::new()
$imports = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($file in $files) {
    $headers = & $ObjDump -p $file.FullName 2>&1
    if ($LASTEXITCODE -ne 0) {
        $problems.Add("objdump failed for $($file.FullName)")
        continue
    }
    if (-not ($headers -match 'file format pei-x86-64')) {
        $problems.Add("not a 64-bit Windows binary: $($file.FullName)")
    }
    foreach ($line in $headers) {
        if ($line -notmatch 'DLL Name:\s*(\S+)') { continue }
        $name = $Matches[1]
        [void]$imports.Add($name)
        $lower = $name.ToLowerInvariant()
        if ($bundled.ContainsKey($lower)) { continue }
        if ($lower -like 'api-ms-win-*' -or $lower -like 'ext-ms-win-*') { continue }

        # A future compiler switch to MSVC must ship or install its runtime.
        # Do not let a developer's machine-wide redist hide that dependency.
        if ($lower -match '^(vcruntime|msvcp|concrt).*\.dll$') {
            $problems.Add("Microsoft C++ runtime is imported but not bundled: $name ($($file.Name))")
            continue
        }

        if (-not (Test-Path -LiteralPath (Join-Path $env:WINDIR "System32\$name") -PathType Leaf)) {
            $problems.Add("unresolved DLL import: $name ($($file.Name))")
        }
    }
}
if ($problems.Count) {
    throw ($problems -join [Environment]::NewLine)
}

$checkOut = Join-Path ([IO.Path]::GetTempPath()) ("qol-installcheck-" + [guid]::NewGuid().ToString('N') + '.out')
$checkErr = "$checkOut.err"
try {
    $process = Start-Process -FilePath (Join-Path $Dist 'QualityOfLifeModManager.exe') `
        -ArgumentList '-installcheck' -WorkingDirectory $Dist -Wait -PassThru `
        -WindowStyle Hidden -RedirectStandardOutput $checkOut -RedirectStandardError $checkErr
    $output = ((Get-Content -LiteralPath $checkOut -Raw -ErrorAction SilentlyContinue) +
               (Get-Content -LiteralPath $checkErr -Raw -ErrorAction SilentlyContinue)).Trim()
    if ($process.ExitCode -ne 0) {
        throw "staged runtime check failed with exit $($process.ExitCode): $output"
    }
    Write-Host $output
}
finally {
    Remove-Item -LiteralPath $checkOut, $checkErr -Force -ErrorAction SilentlyContinue
}

Write-Host "Runtime closure: $($files.Count) PE files, $($imports.Count) imported DLL names, all resolved."

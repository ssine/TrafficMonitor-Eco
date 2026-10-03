param([string]$MfcRoot = "", [string]$Output = "", [string]$ConfigPath = "")
$ErrorActionPreference = "Stop"
$repo = Split-Path $PSScriptRoot -Parent
$checkout = $repo
if (!$Output) { $Output = Join-Path $repo 'eco-release' }
# Upstream mixes UTF-8 without BOM and legacy Chinese source files. Compile a
# normalized staging copy, leaving the checkout untouched on any system locale.
New-Item -ItemType Directory -Force $Output | Out-Null
$stage = Join-Path $Output '_source'
New-Item -ItemType Directory -Force $stage | Out-Null
Get-ChildItem -LiteralPath $repo -Force | Where-Object { $_.Name -notin @('.git', 'Bin', 'eco-release', '_source') -and $_.FullName -ne $Output } | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $stage -Recurse -Force
}
$strictUtf8 = New-Object System.Text.UTF8Encoding($false, $true)
$utf8Bom = New-Object System.Text.UTF8Encoding($true)
Get-ChildItem -LiteralPath $stage -Recurse -File | Where-Object { $_.Extension -in @('.h', '.cpp', '.inl', '.rc', '.rc2') } | ForEach-Object {
    $bytes = [IO.File]::ReadAllBytes($_.FullName)
    if ($bytes.Length -gt 2 -and $bytes[0] -eq 239 -and $bytes[1] -eq 187 -and $bytes[2] -eq 191) { return }
    if ($bytes.Length -ge 4 -and $bytes[0] -eq 255 -and $bytes[1] -eq 254 -and $bytes[2] -eq 0 -and $bytes[3] -eq 0) {
        $text = [Text.Encoding]::UTF32.GetString($bytes, 4, $bytes.Length - 4)
    } elseif ($bytes.Length -ge 2 -and $bytes[0] -eq 255 -and $bytes[1] -eq 254) {
        $text = [Text.Encoding]::Unicode.GetString($bytes, 2, $bytes.Length - 2)
    } elseif ($bytes.Length -ge 2 -and $bytes[0] -eq 254 -and $bytes[1] -eq 255) {
        $text = [Text.Encoding]::BigEndianUnicode.GetString($bytes, 2, $bytes.Length - 2)
    } else {
        try { $text = $strictUtf8.GetString($bytes) } catch { $text = [Text.Encoding]::GetEncoding(936).GetString($bytes) }
    }
    [IO.File]::WriteAllText($_.FullName, $text, $utf8Bom)
}
$repo = $stage
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio C++ Build Tools were not found.' }
$msbuild = Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
$properties = @('/nologo', '/t:Rebuild', '/m:4', '/v:minimal', '/p:Configuration=Release (lite)', '/p:Platform=x64', '/p:PlatformToolset=v143', "/p:SolutionDir=$repo\")
if ($MfcRoot) {
    $properties += @("/p:VC_ATLMFC_IncludePath=$MfcRoot\include", "/p:VC_LibraryPath_ATL_x64=$MfcRoot\lib\x64", "/p:MFC_KeyFile=$MfcRoot\lib\x64\mfcs140u.lib")
}
Push-Location $repo
try {
    & $msbuild (Join-Path $repo 'TrafficMonitor\TrafficMonitor.vcxproj') @properties
    if ($LASTEXITCODE) { throw "TrafficMonitor build failed: $LASTEXITCODE" }
    New-Item -ItemType Directory -Force $Output,(Join-Path $Output 'plugins') | Out-Null
    $bin = Join-Path $repo 'Bin\x64\Release (lite)'
    Copy-Item (Join-Path $bin 'TrafficMonitor.exe') $Output
    $devcmd = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
    $lines = @(
        '@echo off',
        "call `"$devcmd`" >nul",
        "cd /d `"$PSScriptRoot`"",
        "cl /nologo /std:c++17 /utf-8 /O2 /MT /EHsc /LD /DUNICODE /D_UNICODE EcoTelemetry.cpp /Fo:`"$Output\EcoTelemetry.obj`" /Fe:`"$Output\plugins\EcoTelemetry.dll`" /link setupapi.lib pdh.lib",
        'if errorlevel 1 exit /b 1',
        "cl /nologo /std:c++17 /utf-8 /O2 /MT /EHsc /DECO_PROBE /DUNICODE /D_UNICODE EcoTelemetry.cpp /Fo:`"$Output\TelemetryProbe.obj`" /Fe:`"$Output\TelemetryProbe.exe`" /link setupapi.lib pdh.lib",
        'exit /b %errorlevel%'
    )
    $batch = Join-Path $Output 'compile-telemetry.cmd'
    Set-Content -LiteralPath $batch -Value $lines -Encoding ASCII
    & $env:ComSpec /d /c $batch
    if ($LASTEXITCODE) { throw "Telemetry build failed: $LASTEXITCODE" }
    # App-local release runtimes: no installer or machine-wide change.
    $crtBase = Join-Path $vs 'VC\Redist\MSVC'
    $crtVersion = Get-ChildItem $crtBase -Directory | Where-Object { $_.Name -match '^([0-9]+\.)+[0-9]+$' } | Sort-Object { [version]$_.Name } | Select-Object -Last 1
    $crt = Join-Path $crtVersion.FullName 'x64\Microsoft.VC143.CRT'
    foreach ($name in @('msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll')) {
        Copy-Item (Join-Path $crt $name) $Output
    }
    $mfcVcRoot = Join-Path $vs 'VC'
    if ($MfcRoot) { $mfcVcRoot = Split-Path (Split-Path (Split-Path (Split-Path $MfcRoot))) }
    $mfcRuntime = Get-ChildItem (Join-Path $mfcVcRoot 'Redist\MSVC') -Filter 'mfc140u.dll' -Recurse |
        Where-Object { $_.FullName -match '\\x64\\Microsoft\.VC143\.MFC\\' } | Select-Object -First 1
    if (!$mfcRuntime) { throw 'The x64 release MFC runtime was not found.' }
    Copy-Item $mfcRuntime.FullName $Output
    Copy-Item (Join-Path $checkout 'LICENSE'),(Join-Path $checkout 'LICENSE_CN') $Output
    Copy-Item (Join-Path $PSScriptRoot 'README.txt') (Join-Path $Output 'README-Eco.txt')
    Set-Content (Join-Path $Output 'global_cfg.ini') -Value "[config]`r`nportable_mode = true" -Encoding UTF8
    if ($ConfigPath) { Copy-Item -LiteralPath $ConfigPath (Join-Path $Output 'config.ini') }
} finally { Pop-Location }

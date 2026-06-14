param(
    [string]$TrialName = '',
    [string[]]$TrialFeatures = @(),
    [switch]$TrialJelly
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$vcvars = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$outDir = Join-Path $projectRoot 'build'
$checkDir = Join-Path $outDir 'pbjelly'
$publicOut = Join-Path $outDir 'public'
$checkPublicOut = Join-Path $checkDir 'public'

New-Item -ItemType Directory -Force -Path $outDir | Out-Null
Remove-Item -LiteralPath (Join-Path $outDir 'pbcheck') -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $checkDir | Out-Null
New-Item -ItemType Directory -Force -Path $publicOut | Out-Null
New-Item -ItemType Directory -Force -Path $checkPublicOut | Out-Null
Remove-Item -LiteralPath `
    (Join-Path $outDir 'portblaster.exe'), `
    (Join-Path $outDir 'portblaster-check.exe'), `
    (Join-Path $outDir 'portblaster-check.pdb'), `
    (Join-Path $outDir 'pbcheck.exe'), `
    (Join-Path $outDir 'pbprobe.exe'), `
    (Join-Path $outDir 'pbjelly.exe') `
    -Force -ErrorAction SilentlyContinue
Copy-Item -Path (Join-Path $projectRoot 'public\*') -Destination $publicOut -Recurse -Force
Copy-Item -Path (Join-Path $projectRoot 'public\*') -Destination $checkPublicOut -Recurse -Force

if (-not (Test-Path -LiteralPath $vcvars)) {
    throw "Could not find Visual Studio vcvars64.bat at $vcvars"
}

function Get-FeatureDefines {
    param([string[]]$Features)
    $defs = @()
    foreach ($feature in $Features) {
        $name = $feature.ToUpperInvariant()
        if ($name -eq 'LOG') { $defs += '/DPB_FEAT_LOG=1' }
        elseif ($name -eq 'METRICS') { $defs += '/DPB_FEAT_METRICS=1' }
        elseif ($name -eq 'STREAM') { $defs += '/DPB_FEAT_STREAM=1' }
        elseif ($name -eq 'RANGE') { $defs += '/DPB_FEAT_RANGE=1' }
        elseif ($name -eq 'MIME_PLUS') { $defs += '/DPB_FEAT_MIME_PLUS=1' }
        elseif ($name -eq 'COPY_URL') { $defs += '/DPB_FEAT_COPY_URL=1' }
        elseif ($name -eq 'BROWSE') { $defs += '/DPB_FEAT_BROWSE=1' }
        elseif ($name -eq 'TIMEOUT') { $defs += '/DPB_FEAT_TIMEOUT=1' }
        elseif ($name -eq 'STATUS_ENDPOINT') { $defs += '/DPB_FEAT_STATUS_ENDPOINT=1' }
        elseif ($name -eq 'ACCESS_LOG') { $defs += '/DPB_FEAT_ACCESS_LOG=1' }
        elseif ($name -eq 'CONFIG') { $defs += '/DPB_FEAT_CONFIG=1' }
        elseif ($name -eq 'DIR_LIST') { $defs += '/DPB_FEAT_DIR_LIST=1' }
        elseif ($name -eq 'BIND_ALL') { $defs += '/DPB_FEAT_BIND_ALL=1' }
        elseif ($name -eq 'IPV6') { $defs += '/DPB_FEAT_IPV6=1' }
        elseif ($name -eq 'TRAY') { $defs += '/DPB_FEAT_TRAY=1' }
        elseif ($name -eq 'THREADS') { $defs += '/DPB_FEAT_THREADS=1' }
        elseif ($name -eq 'WORKERS') { $defs += '/DPB_FEAT_WORKERS=1' }
        elseif ($name -eq 'DEFENSE') { $defs += '/DPB_FEAT_DEFENSE=1' }
        elseif ($name -eq 'WIN2K_UI') { $defs += '/DPB_FEAT_WIN2K_UI=1' }
        elseif ($name -ne '') { throw "Unknown feature '$feature'. Use LOG, METRICS, STREAM, RANGE, MIME_PLUS, COPY_URL, BROWSE, TIMEOUT, STATUS_ENDPOINT, ACCESS_LOG, CONFIG, DIR_LIST, BIND_ALL, IPV6, TRAY, THREADS, WORKERS, DEFENSE, WIN2K_UI." }
    }
    return ($defs -join ' ')
}

function Invoke-ServerBuild {
    param(
        [string]$Name,
        [int]$TargetKb,
        [string[]]$Features,
        [bool]$Jelly,
        [bool]$Debug
    )

    $featureDefs = Get-FeatureDefines $Features
    $outPath = if ($Jelly) { "build\pbjelly\$Name.exe" } else { "build\$Name.exe" }
    $debugDefs = if ($Jelly) { '/DPB_FEAT_JELLY=1 /DPORTBLASTER_CHECK' } else { '' }
    if ($Debug) {
        cmd /c "`"$vcvars`" >nul && cl /nologo /TC /Od /Zi /GS- /DPB_TARGET_KB=$TargetKb $featureDefs $debugDefs portblaster.c /Fe:$outPath /link /DEBUG:FULL /NODEFAULTLIB user32.lib ws2_32.lib kernel32.lib shell32.lib ole32.lib /ENTRY:WinMainCRTStartup /SUBSYSTEM:WINDOWS /INCREMENTAL:NO"
    } else {
        cmd /c "`"$vcvars`" >nul && cl /nologo /TC /O1 /GS- /DPB_TARGET_KB=$TargetKb $featureDefs $debugDefs portblaster.c /Fe:$outPath /link /NODEFAULTLIB user32.lib ws2_32.lib kernel32.lib shell32.lib ole32.lib /ENTRY:WinMainCRTStartup /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /INCREMENTAL:NO"
    }
}

Push-Location $projectRoot
try {
    Invoke-ServerBuild 'pb20' 20 @() $false $false
    Invoke-ServerBuild 'pb50' 50 @('LOG', 'METRICS', 'MIME_PLUS', 'COPY_URL', 'BROWSE') $false $false
    Invoke-ServerBuild 'pb100' 100 @('LOG', 'METRICS', 'STREAM', 'RANGE', 'MIME_PLUS', 'COPY_URL', 'BROWSE', 'TIMEOUT', 'STATUS_ENDPOINT', 'ACCESS_LOG', 'CONFIG', 'DIR_LIST', 'BIND_ALL', 'IPV6', 'TRAY', 'WORKERS', 'DEFENSE', 'WIN2K_UI') $false $false
    Invoke-ServerBuild 'pbj20' 20 @() $true $true
    Invoke-ServerBuild 'pbj50' 50 @('LOG', 'METRICS', 'MIME_PLUS', 'COPY_URL', 'BROWSE') $true $true
    Invoke-ServerBuild 'pbj100' 100 @('LOG', 'METRICS', 'STREAM', 'RANGE', 'MIME_PLUS', 'COPY_URL', 'BROWSE', 'TIMEOUT', 'STATUS_ENDPOINT', 'ACCESS_LOG', 'CONFIG', 'DIR_LIST', 'BIND_ALL', 'IPV6', 'TRAY', 'WORKERS', 'DEFENSE', 'WIN2K_UI') $true $true
    if ($TrialName) {
        Invoke-ServerBuild $TrialName 50 $TrialFeatures ([bool]$TrialJelly) ([bool]$TrialJelly)
    }
    cmd /c "`"$vcvars`" >nul && cl /nologo /TC /O1 pbjelly.c /Fe:build\pbjelly\pbjelly.exe /link user32.lib ws2_32.lib iphlpapi.lib shell32.lib"
}
finally {
    Pop-Location
}

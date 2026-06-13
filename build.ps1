$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$vcvars = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$outDir = Join-Path $projectRoot 'build'
$checkDir = Join-Path $outDir 'pbcheck'
$publicOut = Join-Path $outDir 'public'
$checkPublicOut = Join-Path $checkDir 'public'

New-Item -ItemType Directory -Force -Path $outDir | Out-Null
New-Item -ItemType Directory -Force -Path $checkDir | Out-Null
New-Item -ItemType Directory -Force -Path $publicOut | Out-Null
New-Item -ItemType Directory -Force -Path $checkPublicOut | Out-Null
Remove-Item -LiteralPath `
    (Join-Path $outDir 'portblaster.exe'), `
    (Join-Path $outDir 'portblaster-check.exe'), `
    (Join-Path $outDir 'portblaster-check.pdb'), `
    (Join-Path $outDir 'pbcheck.exe'), `
    (Join-Path $outDir 'pbprobe.exe') `
    -Force -ErrorAction SilentlyContinue
Copy-Item -Path (Join-Path $projectRoot 'public\*') -Destination $publicOut -Recurse -Force
Copy-Item -Path (Join-Path $projectRoot 'public\*') -Destination $checkPublicOut -Recurse -Force

if (-not (Test-Path -LiteralPath $vcvars)) {
    throw "Could not find Visual Studio vcvars64.bat at $vcvars"
}

Push-Location $projectRoot
try {
    cmd /c "`"$vcvars`" >nul && cl /nologo /TC /O1 /GS- /DPB_TARGET_KB=20 server_gui.c /Fe:build\pb20.exe /link /NODEFAULTLIB user32.lib ws2_32.lib kernel32.lib /ENTRY:WinMainCRTStartup /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /INCREMENTAL:NO"
    cmd /c "`"$vcvars`" >nul && cl /nologo /TC /O1 /GS- /DPB_TARGET_KB=50 server_gui.c /Fe:build\pb50.exe /link /NODEFAULTLIB user32.lib ws2_32.lib kernel32.lib /ENTRY:WinMainCRTStartup /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /INCREMENTAL:NO"
    cmd /c "`"$vcvars`" >nul && cl /nologo /TC /O1 /GS- /DPB_TARGET_KB=100 server_gui.c /Fe:build\pb100.exe /link /NODEFAULTLIB user32.lib ws2_32.lib kernel32.lib /ENTRY:WinMainCRTStartup /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /INCREMENTAL:NO"
    cmd /c "`"$vcvars`" >nul && cl /nologo /TC /Od /Zi /GS- /DPORTBLASTER_CHECK /DPB_TARGET_KB=20 server_gui.c /Fe:build\pbcheck\pb20.exe /link /DEBUG:FULL /NODEFAULTLIB user32.lib ws2_32.lib kernel32.lib /ENTRY:WinMainCRTStartup /SUBSYSTEM:WINDOWS /INCREMENTAL:NO"
    cmd /c "`"$vcvars`" >nul && cl /nologo /TC /Od /Zi /GS- /DPORTBLASTER_CHECK /DPB_TARGET_KB=50 server_gui.c /Fe:build\pbcheck\pb50.exe /link /DEBUG:FULL /NODEFAULTLIB user32.lib ws2_32.lib kernel32.lib /ENTRY:WinMainCRTStartup /SUBSYSTEM:WINDOWS /INCREMENTAL:NO"
    cmd /c "`"$vcvars`" >nul && cl /nologo /TC /Od /Zi /GS- /DPORTBLASTER_CHECK /DPB_TARGET_KB=100 server_gui.c /Fe:build\pbcheck\pb100.exe /link /DEBUG:FULL /NODEFAULTLIB user32.lib ws2_32.lib kernel32.lib /ENTRY:WinMainCRTStartup /SUBSYSTEM:WINDOWS /INCREMENTAL:NO"
    cmd /c "`"$vcvars`" >nul && cl /nologo /TC /O1 pbprobe.c /Fe:build\pbcheck\pbprobe.exe /link user32.lib ws2_32.lib"
}
finally {
    Pop-Location
}

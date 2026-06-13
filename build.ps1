$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$vcvars = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$outDir = Join-Path $projectRoot 'build'
$publicOut = Join-Path $outDir 'public'

New-Item -ItemType Directory -Force -Path $outDir | Out-Null
New-Item -ItemType Directory -Force -Path $publicOut | Out-Null
Copy-Item -Path (Join-Path $projectRoot 'public\*') -Destination $publicOut -Recurse -Force

if (-not (Test-Path -LiteralPath $vcvars)) {
    throw "Could not find Visual Studio vcvars64.bat at $vcvars"
}

Push-Location $projectRoot
try {
    cmd /c "`"$vcvars`" >nul && cl /nologo /TC /O1 /GS- server_gui.c /Fe:build\portblaster.exe /link /NODEFAULTLIB user32.lib ws2_32.lib kernel32.lib /ENTRY:WinMainCRTStartup /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /INCREMENTAL:NO"
    cmd /c "`"$vcvars`" >nul && cl /nologo /TC /Od /Zi /GS- /DPORTBLASTER_CHECK server_gui.c /Fe:build\portblaster-check.exe /link /DEBUG:FULL /NODEFAULTLIB user32.lib ws2_32.lib kernel32.lib /ENTRY:WinMainCRTStartup /SUBSYSTEM:WINDOWS /INCREMENTAL:NO"
    cmd /c "`"$vcvars`" >nul && cl /nologo /TC /O1 pbcheck.c /Fe:build\pbcheck.exe /link user32.lib ws2_32.lib"
}
finally {
    Pop-Location
}

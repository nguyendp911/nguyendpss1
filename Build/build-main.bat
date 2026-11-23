@echo off
setlocal
call C:\Muemu\msvc-dl\msvc\setup_x86.bat
set OUT=C:\Muemu\build\Main
if exist "%OUT%" rmdir /s /q "%OUT%"
mkdir "%OUT%"
cd /d C:\Muemu\Source\Client\Main
set CLFLAGS=/nologo /c /O2 /Oi /Gy /GL /W3 /EHsc /MD /Zc:wchar_t /DWIN32 /DGLEW_STATIC /DNDEBUG /D_WINDOWS /D_USRDLL /DMAIN_EXPORTS /D_MBCS /Fo"%OUT%\\" /Fd"%OUT%\vc.pdb"
echo === PCH
cl %CLFLAGS% /Yc"stdafx.h" /Fp"%OUT%\stdafx.pch" stdafx.cpp
if errorlevel 1 exit /b 1
echo === stb
cl %CLFLAGS% stb.cpp
if errorlevel 1 exit /b 1
echo === Compile
cl %CLFLAGS% /Yu"stdafx.h" /Fp"%OUT%\stdafx.pch" @C:\Muemu\build\main-use.rsp
if errorlevel 1 exit /b 1
echo === Resources
rc /nologo /fo"%OUT%\Main.res" Main.rc
if errorlevel 1 exit /b 1
echo === Link
link /nologo /DLL /LTCG /OPT:REF /OPT:ICF /SUBSYSTEM:WINDOWS /NODEFAULTLIB:libcmt.lib /OUT:"%OUT%\Main.dll" "%OUT%\*.obj" "%OUT%\Main.res" user32.lib shell32.lib advapi32.lib gdi32.lib ole32.lib wininet.lib mfplat.lib mfreadwrite.lib mfuuid.lib
if errorlevel 1 exit /b 1
echo BUILD OK: %OUT%\Main.dll




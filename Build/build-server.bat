@echo off
rem Usage: build-server.bat <ProjectName> <MYSQL|ODBC>
rem Builds C:\Muemu\Source\MuServer\<ProjectName> with the portable MSVC (x86, Release).
setlocal enabledelayedexpansion
set PROJ=%~1
set MODE=%~2
set RT=%~3
if "%RT%"=="" set RT=MD
call C:\Muemu\msvc-dl\msvc\setup_x86.bat
set INCLUDE=%INCLUDE%;%VCToolsInstallDir%atlmfc\include
set LIB=%LIB%;%VCToolsInstallDir%atlmfc\lib\x86
set SD=C:\Muemu\Source\MuServer
set OUT=C:\Muemu\build\%PROJ%
if exist "%OUT%" rmdir /s /q "%OUT%"
mkdir "%OUT%"
cd /d "%SD%\%PROJ%"

set DEFS=/DWIN32 /DNDEBUG /D_MBCS
set INCS=
set LIBPATH=
if /i "%MODE%"=="MYSQL" (
  set DEFS=%DEFS% /DMYSQL
  set INCS=/I"%SD%\Dependencies\mysql-connector-8.0.33\Release\include\jdbc"
  set LIBPATH=/LIBPATH:"%SD%\Dependencies\mysql-connector-8.0.33\Release\lib"
)

set CLFLAGS=/nologo /c /O2 /Oi /Gy /GL /W3 /EHsc /%RT% /Zc:wchar_t /Fo"%OUT%\\" /Fd"%OUT%\\vc.pdb" %DEFS% %INCS%

echo === PCH
cl %CLFLAGS% /Yc"stdafx.h" /Fp"%OUT%\stdafx.pch" stdafx.cpp
if errorlevel 1 exit /b 1

echo === Compile
set SRCS=
for %%f in (*.cpp) do (
  if /i not "%%f"=="stdafx.cpp" set SRCS=!SRCS! "%%f"
)
cl %CLFLAGS% /Yu"stdafx.h" /Fp"%OUT%\stdafx.pch" !SRCS!
if errorlevel 1 exit /b 1

echo === Resources
if exist %PROJ%.rc rc /nologo /fo"%OUT%\%PROJ%.res" %PROJ%.rc
if errorlevel 1 exit /b 1

echo === Link
set RES=
if exist "%OUT%\%PROJ%.res" set RES="%OUT%\%PROJ%.res"
link /nologo /LTCG /OPT:REF /OPT:ICF /SUBSYSTEM:CONSOLE /OUT:"%OUT%\%PROJ%.exe" "%OUT%\*.obj" %RES% user32.lib gdi32.lib odbc32.lib advapi32.lib %LIBPATH%
if errorlevel 1 exit /b 1
echo BUILD OK: %OUT%\%PROJ%.exe

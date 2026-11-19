@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars32.bat"
if errorlevel 1 exit /b 1
cd /d "%~dp0"
cl.exe /nologo /O2 /EHsc /MT /LD peggle_online.cpp /link /DEF:bass.def /OUT:bass.dll /MACHINE:X86 ws2_32.lib user32.lib winhttp.lib
if errorlevel 1 exit /b 1
cl.exe /nologo /O2 /EHsc /MT launcher.cpp /link /OUT:PlayPeggleOnline.exe /SUBSYSTEM:WINDOWS /MACHINE:X86 user32.lib gdi32.lib comctl32.lib shell32.lib advapi32.lib ole32.lib winhttp.lib
if errorlevel 1 exit /b 1

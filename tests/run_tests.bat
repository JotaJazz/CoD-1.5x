@echo off
rem Builds and runs the tests. Needs a Visual Studio x86 developer prompt (cl.exe on PATH).
setlocal
cd /d "%~dp0"
set CLFLAGS=/nologo /EHsc /W3 /D_CRT_SECURE_NO_WARNINGS

cl %CLFLAGS% unit_test.cpp ..\sha256.cpp ..\fov.cpp ..\names.cpp ..\update.cpp >build.log 2>&1 || (type build.log & exit /b 1)
unit_test.exe || (echo unit_test FAILED & exit /b 1)

cl %CLFLAGS% crash_test.cpp ..\crash.cpp >build.log 2>&1 || (type build.log & exit /b 1)
if exist crashes rmdir /s /q crashes
crash_test.exe
if not exist crashes\crash-*.txt (echo crash_test FAILED: no report & exit /b 1)
if not exist crashes\crash-*.dmp (echo crash_test FAILED: no minidump & exit /b 1)
findstr /c:"ACCESS_VIOLATION (write to address 0x00000000) at crash_test.exe+" crashes\crash-*.txt >nul || (echo crash_test FAILED: wrong summary & type crashes\crash-*.txt & exit /b 1)
echo crash_test ok

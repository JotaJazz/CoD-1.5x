@echo off
rem CoD 1.5x setup: install / update / uninstall. Put it in the Call of Duty folder with cod15x.dll.
rem Usage: install.bat            (menu)
rem        install.bat /install   or   install.bat /uninstall
setlocal
cd /d "%~dp0"
title CoD 1.5x setup

if not exist "CoDMP.exe" (
	echo This folder has no CoDMP.exe.
	echo Copy install.bat and cod15x.dll into your Call of Duty folder and run it again.
	goto end
)

set ACTION=%~1
if /i "%ACTION%"=="/install" goto check
if /i "%ACTION%"=="/uninstall" goto check
echo.
echo   CoD 1.5x
echo   1 - Install / update
echo   2 - Uninstall
echo.
choice /c 12 /n /m "Choose 1 or 2: "
if errorlevel 2 (set ACTION=/uninstall) else (set ACTION=/install)

:check
rem the game folder may need administrator rights (Program Files): relaunch elevated
(echo.> ".cod15x_write_test") 2>nul && del ".cod15x_write_test" || (
	echo Asking for administrator rights...
	powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -ArgumentList '%ACTION%' -Verb RunAs"
	exit /b
)
if /i "%ACTION%"=="/uninstall" goto uninstall

:install
if not exist "cod15x.dll" (
	echo cod15x.dll is missing: put it next to install.bat.
	goto end
)
set RENAMED=
if not exist "mss32_original.dll" (
	if not exist "mss32.dll" (
		echo mss32.dll was not found. Repair or verify the game files first.
		goto end
	)
	rem our DLL contains the text "mss32_original.dll"; the original Miles DLL does not
	findstr /m /c:"mss32_original.dll" "mss32.dll" >nul && (
		echo mss32.dll is already CoD 1.5x, but mss32_original.dll is missing.
		echo Restore the original mss32.dll from your game, then run this again.
		goto end
	)
	ren "mss32.dll" "mss32_original.dll" || goto failed
	set RENAMED=1
)
copy /y "cod15x.dll" "mss32.dll" >nul || (
	if defined RENAMED ren "mss32_original.dll" "mss32.dll"
	goto failed
)
echo CoD 1.5x installed. Start multiplayer: the bottom-right corner shows 1.5x.
goto end

:uninstall
if not exist "mss32_original.dll" (
	echo CoD 1.5x is not installed in this folder.
	goto end
)
if exist "mss32.dll" (
	findstr /m /c:"mss32_original.dll" "mss32.dll" >nul || (
		echo mss32.dll is not CoD 1.5x, nothing was changed.
		goto end
	)
	del /f "mss32.dll" || goto failed
)
ren "mss32_original.dll" "mss32.dll" || goto failed
echo CoD 1.5x removed. The original mss32.dll is back.
goto end

:failed
echo Could not change the files. Close the game and try again.

:end
echo.
pause

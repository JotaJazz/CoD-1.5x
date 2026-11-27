@echo off
set GAME=C:\Program Files (x86)\Steam\steamapps\common\Call of Duty
cd Dev
rem keep the real Miles DLL: rename it once before our mss32.dll replaces it
if not exist "%GAME%\mss32_original.dll" ren "%GAME%\mss32.dll" mss32_original.dll
copy mss32.dll "%GAME%"

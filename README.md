# Call of Duty 1.5x [Client]

CoDExtended Client (1.5x), is a client-side modification for Call of Duty 1.5.

## Changes/additions

- new UI element (cg_xui_fps)
- Discord Rich Presence
- unlocked FOV,thirdperson and thirdperson angle
- Support for disabling jump slowdown
- native desktop resolution as `r_mode 10` (shown in the video menu) and `r_mode 11`
- Hor+ widescreen FOV (`cg_widescreenfov`, default 1): `cg_fov` is the 4:3 horizontal FOV
- raw mouse input (`m_rawinput 1`), ignores Windows pointer speed and acceleration
- blocks cgame/ui/game DLLs that are not the official 1.5 ones (a server can ship them in a downloaded pk3); `cl_allowCustomDlls 1` allows them
- fullscreen native modes use the desktop refresh rate
- bugfixes

## Building from source

Requires Visual Studio 2019 or newer with the "Desktop development with C++" workload (v142 toolset).

```
MSBuild cod5x.vcxproj /p:Configuration=Release /p:Platform=Win32
```

The build output is `Release/mss32.dll`. No game binary is modified:

1. In the game directory, rename the original `mss32.dll` (Miles Sound System) to `mss32_original.dll`.
2. Copy `Release/mss32.dll` into the game directory.

To uninstall, delete `mss32.dll` and rename `mss32_original.dll` back to `mss32.dll`.
CoDSP.exe and the United Offensive exes also load `mss32.dll`; for them it only forwards to the original.
Older installs with a patched `CoDMP.exe` importing `cod5x.dll` keep working if the file is named `cod5x.dll`.

## Notice

Forum: http://xtnded.org  
Discord: http://discord.cod1x.eu  

## License

CoDExtended Client is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

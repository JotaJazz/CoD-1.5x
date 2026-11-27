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
- bugfixes

## Building from source

Requires Visual Studio 2019 or newer with the "Desktop development with C++" workload (v142 toolset).

```
MSBuild cod5x.vcxproj /p:Configuration=Release /p:Platform=Win32
```

Copy `Release/cod5x.dll` to the game directory. `CoDMP.exe` must import `cod5x.dll` instead of `mss32.dll` (the release exe has exactly that 5-byte change).

## Notice

Forum: http://xtnded.org  
Discord: http://discord.cod1x.eu  

## License

CoDExtended Client is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

# VoidCare (VoidTools)

Windows x64 offline optimization, health, and security multi-tool.

Primary UI: ImGui (`VoidCare.exe`)  
Optional experimental UI: WPF + bridge (disabled by default)

**Developed by Ysf (Lone Wolf Developer)**

## Platform and Safety

- Windows 10/11 x64 only
- Administrator launch required (`requireAdministrator` manifest)
- Offline-only behavior (no HTTP/web API usage)
- No custom antivirus engine
- Defender-only auto-remediation, and only for Defender-detected threats
- Suspicious-file scanner is heuristic-only and may produce false positives
- Suspicious files are never auto-deleted
- Default suspicious action is quarantine-first
- Destructive actions require confirmation and restore-point attempt

## Build Requirements

- Visual Studio 2022 (MSVC x64)
- CMake 3.24+
- Qt 6.6+ SDK (used by core/ui services and deployment)
- PowerShell 5+
- Optional for WPF path: .NET 8 SDK

## Configure

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\configure.ps1 -BuildDir build -BuildType Release -QtDir "C:\Qt\6.8.0\msvc2022_64"
```

Optional experimental WPF/bridge enable:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\configure.ps1 -BuildDir build -BuildType Release -BuildWpf -QtDir "C:\Qt\6.8.0\msvc2022_64"
```

## Build

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -BuildDir build -Configuration Release -BuildTests
```

Optional WPF build in same pass:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -BuildDir build -Configuration Release -BuildTests -BuildWpf
```

## Package (Portable)

Default package (ImGui primary):

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-portable.ps1 -BuildDir build -Configuration Release -OutputDir dist
```

Include optional experimental WPF artifacts:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-portable.ps1 -BuildDir build -Configuration Release -OutputDir dist -IncludeWpf
```

Portable distribution is a folder/zip with `VoidCare.exe` plus runtime DLLs (`windeployqt`).  
It is not a single-file EXE.

## Discord Rich Presence

- Local IPC only: `\\?\pipe\discord-ipc-0` .. `\\?\pipe\discord-ipc-9`
- Requires local Discord desktop client running
- Build-time client id variable: `VOIDCARE_DISCORD_CLIENT_ID`
- Current default client id: `1479026910574153921`
- No web calls are used for RPC

## Repo Layout

- `app/` ImGui frontend (primary)
- `core/`
- `platform/windows/`
- `ui/` C++ AppController and optional WPF project
- `bridge/` optional bridge executable (for WPF path)
- `scripts/`
- `tests/`

# VoidCare (VoidTools)

Windows x64 offline optimization, health, and security multi-tool.
Frontend: WPF (`net8.0-windows`, x64).
Backend: C++20 services over local JSONL bridge (`VoidCare.Bridge.exe`).

**Developed by Ysf (Lone Wolf Developer)**

## Platform

- Windows 10/11 x64 only
- Administrator launch required
- Offline-only behavior (no HTTP/web API usage)

## Safety Rules

- No custom antivirus engine
- Defender-only auto-remediation, and only for Defender-detected threats
- Suspicious scan is heuristic-only and may produce false positives
- Suspicious files are never auto-deleted
- Default action is quarantine-first
- Destructive actions require confirmation and restore-point attempt

## Build Requirements

- Visual Studio 2022 (MSVC x64)
- CMake 3.24+
- Qt 6.6+ SDK (for bridge/runtime dependencies)
- .NET SDK 8+
- PowerShell 5+

## Build

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\configure.ps1 -BuildDir build -BuildType Release -QtDir "C:\Qt\6.8.0\msvc2022_64"
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -BuildDir build -Configuration Release -BuildTests
```

## Package (Portable)

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-portable.ps1 -BuildDir build -Configuration Release -OutputDir dist
```

Distribution format is portable folder/zip (`VoidCare.exe` + `VoidCare.Bridge.exe` + required runtime files). It is not a single-file EXE.

## Discord Rich Presence

- Local IPC only: `\\?\pipe\discord-ipc-0` .. `\\?\pipe\discord-ipc-9`
- Requires Discord desktop client running locally
- Build-time client id variable: `VOIDCARE_DISCORD_CLIENT_ID`
- Current default client id in this repo: `1479026910574153921`

## Layout

- `core/`
- `platform/windows/`
- `bridge/`
- `ui/`
- `scripts/`
- `tests/`

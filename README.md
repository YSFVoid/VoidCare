# VoidCare (VoidTools) - CLI First

Premium Windows x64 offline optimization + security command-line tool.

**Developed by Ysf (Lone Wolf Developer)**

## Safety and Constraints

- Windows 10/11 x64 only
- Offline-only behavior (no HTTP, no downloads, no telemetry)
- No custom antivirus engine
- Defender-only remediation for Defender-detected threats
- Suspicious scan is heuristic-only and can produce false positives
- Suspicious files are never auto-deleted automatically
- Quarantine-first workflow
- Destructive actions require confirmation and restore-point attempt

## Build Requirements

- Visual Studio 2022 (MSVC x64)
- CMake 3.24+
- Qt 6.6+ SDK
- PowerShell 5+

## Configure

CLI-first default:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\configure.ps1 -BuildDir build -BuildType Release -QtDir "C:\Qt\6.8.0\msvc2022_64"
```

Enable optional ImGui UI build too:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\configure.ps1 -BuildDir build -BuildType Release -EnableImGui -QtDir "C:\Qt\6.8.0\msvc2022_64"
```

## Build and Test

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -BuildDir build -Configuration Release -BuildTests
```

## Package (Portable)

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-portable.ps1 -BuildDir build -Configuration Release -OutputDir dist
```

Portable output includes `voidcare.exe` (or `VoidCare.exe` if UI build was selected as primary) plus required runtime DLLs.

## CLI Usage

Global flags:

- `--json` machine-readable output
- `--yes` non-interactive confirmation
- `--dry-run` preview changes without mutating system state

Core:

```powershell
voidcare --help
voidcare --version
voidcare about
voidcare status
```

Security:

```powershell
voidcare security av list
voidcare security defender status
voidcare security scan --quick
voidcare security scan --full
voidcare security scan --path "C:\Users\Administrator\Downloads"
voidcare security remediate
voidcare security persistence list
# Use an exact ID returned by `security persistence list`
voidcare security persistence disable --id "reg:HKCUSoftware\Microsoft\Windows\CurrentVersion\RunDiscord"
voidcare security suspicious scan --quick
voidcare security suspicious scan --full --roots "C:\;D:\"
voidcare security suspicious quarantine list
voidcare security suspicious quarantine --ids 1,2,3 --quick
voidcare security suspicious restore --id 2
voidcare security suspicious restore --id 2 --to "C:\RestoreTarget"
voidcare security suspicious delete --id 2
```

Optimization:

```powershell
voidcare optimize safe
voidcare optimize safe --days 5
voidcare optimize safe --days 5 --include-windows-temp
voidcare optimize power --high
voidcare optimize startup report
```

## Important Limitations

- `security remediate` only runs Defender remediation (`Remove-MpThreat`) and does not perform arbitrary delete actions.
- Suspicious scan uses heuristic scoring; flagged entries are not confirmed malware unless Defender explicitly detects them.
- `--dry-run` is enforced for mutating commands and prints planned actions.
- Some commands require administrator privileges; tool reports when elevation is needed.

## Repo Layout

- `cli/` CLI executable and CLI helpers
- `core/` shared services (scan, persistence, suspicious, optimize, restore, process)
- `platform/windows/` Windows helpers (admin, signature, hashing, paths)
- `app/` optional ImGui frontend
- `bridge/` optional bridge target
- `scripts/` configure/build/package scripts
- `tests/` unit/integration tests

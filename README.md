# VoidCare

Premium Windows x64 offline optimization and security terminal tool.

Developed by Ysf (Lone Wolf Developer)

## Overview

- Terminal-only Windows utility built with C# .NET 8.
- Offline-only runtime behavior: no HTTP, no downloads, no telemetry.
- Windows x64 only.
- Single shipped product: `voidcare.exe`.
- Defender-only remediation. VoidCare does not claim malware unless Microsoft Defender explicitly detects it.

## Safety Rules

- Suspicious-file scanning is heuristic-only and can produce false positives.
- Suspicious files are never auto-deleted by heuristic results.
- Default suspicious-file action is quarantine.
- Permanent delete is a separate command and requires explicit confirmation.
- Admin-only actions detect elevation and warn clearly.
- Restore points are attempted before destructive actions where appropriate.

## Repo Layout

- `src/VoidCare.Cli` entrypoint, command parsing, interactive menu, terminal rendering
- `src/VoidCare.Core` models, catalog data, suspicious-file heuristics
- `src/VoidCare.Infrastructure` Windows integrations, Defender, registry, files, logs, state
- `tests/VoidCare.Tests` unit tests
- `scripts` restore, build, test, and publish helpers

## Requirements

- Windows 10/11 x64
- .NET SDK 8+ for local builds
- PowerShell 5.1+

## Restore

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\restore.ps1
```

## Build

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration Release
```

## Test

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\test.ps1 -Configuration Release
```

## Publish

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\publish.ps1 -Configuration Release -OutputDir dist
```

Exact publish command:

```powershell
dotnet publish src/VoidCare.Cli/VoidCare.Cli.csproj -c Release -r win-x64 --self-contained true /p:PublishSingleFile=true
```

## Usage

Interactive mode:

```powershell
voidcare
voidcare menu
voidcare interactive
```

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
voidcare security persistence disable --ids 1,2,3
voidcare security suspicious scan --quick
voidcare security suspicious scan --full --roots "C:\;D:\"
voidcare security suspicious quarantine --ids 1,2,3
voidcare security suspicious quarantine list
voidcare security suspicious restore --id 1
voidcare security suspicious delete --id 1
```

Optimization:

```powershell
voidcare optimize safe --dry-run
voidcare optimize safe --days 3 --browser-cache
voidcare optimize performance --confirm
voidcare optimize aggressive --confirm --disable-copilot
voidcare optimize startup list
voidcare optimize startup disable --ids 1,2
```

Apps and logs:

```powershell
voidcare apps list --type all
voidcare apps bloat list
voidcare apps bloat remove --ids 1,2 --confirm
voidcare logs show --tail 50
voidcare logs open
```

## Notes

- `security remediate` uses Defender `Remove-MpThreat` only. It does not delete arbitrary files.
- `security suspicious scan` reports suspicious heuristic results, not confirmed malware.
- `--json` emits JSONL progress events for long tasks and a final result or error object.
- Banner/help/about/version screens include the project credits line.

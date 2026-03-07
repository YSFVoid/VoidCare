param(
    [string]$ExePath = "dist\voidcare.exe",
    [int]$CaseTimeoutMs = 45000
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Path $PSScriptRoot -Parent
$exe = Join-Path $repoRoot $ExePath

if (!(Test-Path $exe)) {
    throw "Executable not found at $exe"
}

$tempRoot = Join-Path $env:TEMP "voidcare-smoke"
if (Test-Path $tempRoot) {
    Remove-Item -Recurse -Force $tempRoot
}
New-Item -ItemType Directory -Path $tempRoot | Out-Null
$samplePath = Join-Path $tempRoot "invoice.pdf.exe"
Set-Content -LiteralPath $samplePath -Value "voidcare smoke sample" -NoNewline

$results = New-Object System.Collections.Generic.List[object]

function Format-Arguments {
    param([string[]]$CommandArgs)

    return ($CommandArgs | ForEach-Object {
        if ($_ -match '[\s"]') {
            '"' + ($_ -replace '"', '\"') + '"'
        } else {
            $_
        }
    }) -join " "
}

function Invoke-JsonResult {
    param([string[]]$CommandArgs)

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = ("--json " + (Format-Arguments $CommandArgs)).Trim()
    $psi.WorkingDirectory = $repoRoot
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    [void]$process.Start()
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $process.WaitForExit()

    if ($process.ExitCode -ne 0 -or [string]::IsNullOrWhiteSpace($stdout)) {
        return $null
    }

    $jsonLine = ($stdout -split "`r?`n" | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Last 1)
    if ([string]::IsNullOrWhiteSpace($jsonLine)) {
        return $null
    }

    return $jsonLine | ConvertFrom-Json
}

function Invoke-Case {
    param(
    [string]$Name,
    [string[]]$CommandArgs,
    [string]$InputText = ""
    )

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = Format-Arguments $CommandArgs
    $psi.WorkingDirectory = $repoRoot
    $psi.RedirectStandardInput = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    [void]$process.Start()

    if ($InputText -ne "") {
        $process.StandardInput.Write($InputText)
    }
    $process.StandardInput.Close()

    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()

    $timedOut = -not $process.WaitForExit($CaseTimeoutMs)
    if ($timedOut) {
        try {
            $process.Kill()
        } catch {
        }
        $process.WaitForExit()
    }

    $stdout = $stdoutTask.GetAwaiter().GetResult()
    $stderr = $stderrTask.GetAwaiter().GetResult()

    $results.Add([pscustomobject]@{
        name = $Name
        command = $psi.Arguments
        exitCode = if ($timedOut) { 124 } else { $process.ExitCode }
        ok = (-not $timedOut) -and ($process.ExitCode -eq 0)
        timedOut = $timedOut
        stdoutHead = (($stdout -split "`r?`n") | Select-Object -First 10) -join "`n"
        stderrHead = (($stderr -split "`r?`n") | Select-Object -First 10) -join "`n"
    }) | Out-Null
}

function Invoke-InteractiveCase {
    param(
    [string]$Name,
    [string[]]$InputLines
    )

    $echoChain = ($InputLines | ForEach-Object { "echo $_" }) -join " & "
    $commandText = if ([string]::IsNullOrWhiteSpace($echoChain)) {
        "`"$exe`""
    } else {
        "($echoChain) | `"$exe`""
    }

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = "cmd.exe"
    $psi.Arguments = "/c $commandText"
    $psi.WorkingDirectory = $repoRoot
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    [void]$process.Start()

    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()

    $timedOut = -not $process.WaitForExit($CaseTimeoutMs)
    if ($timedOut) {
        try {
            $process.Kill()
        } catch {
        }
        $process.WaitForExit()
    }

    $stdout = $stdoutTask.GetAwaiter().GetResult()
    $stderr = $stderrTask.GetAwaiter().GetResult()

    $results.Add([pscustomobject]@{
        name = $Name
        command = "<interactive>"
        exitCode = if ($timedOut) { 124 } else { $process.ExitCode }
        ok = (-not $timedOut) -and ($process.ExitCode -eq 0)
        timedOut = $timedOut
        stdoutHead = (($stdout -split "`r?`n") | Select-Object -First 10) -join "`n"
        stderrHead = (($stderr -split "`r?`n") | Select-Object -First 10) -join "`n"
    }) | Out-Null
}

Invoke-Case "help" @("--help")
Invoke-Case "version" @("--version")
Invoke-Case "about" @("about")
Invoke-Case "status" @("status")
Invoke-Case "security-av-list" @("security", "av", "list")
Invoke-Case "security-defender-status" @("security", "defender", "status")
Invoke-Case "security-scan-quick" @("security", "scan", "--quick")
Invoke-Case "security-scan-full" @("security", "scan", "--full")
Invoke-Case "security-scan-path" @("security", "scan", "--path", $tempRoot)
Invoke-Case "security-remediate" @("--yes", "security", "remediate")
Invoke-Case "persistence-list" @("security", "persistence", "list")
Invoke-Case "persistence-disable-dry" @("--dry-run", "--yes", "security", "persistence", "disable", "--id", "1")
Invoke-Case "suspicious-scan-full-temp" @("security", "suspicious", "scan", "--full", "--roots", $tempRoot)
Invoke-Case "suspicious-quarantine-dry" @("--dry-run", "--yes", "security", "suspicious", "quarantine", "--ids", "1")
Invoke-Case "suspicious-quarantine-list-before" @("security", "suspicious", "quarantine", "list")
Invoke-Case "suspicious-quarantine-real" @("--yes", "security", "suspicious", "quarantine", "--ids", "1")
Invoke-Case "suspicious-quarantine-list-after" @("security", "suspicious", "quarantine", "list")
Invoke-Case "suspicious-restore-real" @("--yes", "security", "suspicious", "restore", "--id", "1", "--to", $tempRoot)
Invoke-Case "suspicious-scan-full-temp-2" @("security", "suspicious", "scan", "--full", "--roots", $tempRoot)
Invoke-Case "suspicious-quarantine-real-2" @("--yes", "security", "suspicious", "quarantine", "--ids", "1")
Invoke-Case "suspicious-delete-real" @("--yes", "security", "suspicious", "delete", "--id", "1")
Invoke-Case "optimize-safe-dry" @("--dry-run", "optimize", "safe")
Invoke-Case "optimize-safe-custom-dry" @("--dry-run", "optimize", "safe", "--days", "1", "--include-windows-temp", "--browser-cache")
Invoke-Case "optimize-performance-dry" @("--dry-run", "--yes", "optimize", "performance", "--confirm")
Invoke-Case "optimize-aggressive-dry" @("--dry-run", "--yes", "optimize", "aggressive", "--confirm", "--disable-copilot", "--browser-cache")
Invoke-Case "optimize-startup-list" @("optimize", "startup", "list")
Invoke-Case "optimize-startup-disable-dry" @("--dry-run", "--yes", "optimize", "startup", "disable", "--id", "1")
Invoke-Case "apps-list-all" @("apps", "list", "--type", "all")
Invoke-Case "apps-list-win32" @("apps", "list", "--type", "win32")
Invoke-Case "apps-list-appx" @("apps", "list", "--type", "appx")
Invoke-Case "apps-bloat-list" @("apps", "bloat", "list")
$bloatList = Invoke-JsonResult @("apps", "bloat", "list")
$firstBloatId = if ($null -ne $bloatList -and $null -ne $bloatList.data -and $null -ne $bloatList.data.items -and $bloatList.data.items.Count -gt 0) {
    [string]$bloatList.data.items[0].id
} else {
    $null
}
if ($null -ne $firstBloatId) {
    Invoke-Case "apps-bloat-remove-dry" @("--dry-run", "--yes", "apps", "bloat", "remove", "--ids", $firstBloatId, "--confirm")
} else {
    $results.Add([pscustomobject]@{
        name = "apps-bloat-remove-dry"
        command = "--dry-run --yes apps bloat remove --confirm"
        exitCode = 0
        ok = $true
        timedOut = $false
        stdoutHead = "Skipped: no conservative bloat candidates on this machine."
        stderrHead = ""
    }) | Out-Null
}
Invoke-Case "logs-show" @("logs", "show", "--tail", "10")
Invoke-Case "logs-open" @("logs", "open")
Invoke-InteractiveCase "interactive-main-exit" @("0")
Invoke-InteractiveCase "interactive-main-status-exit" @("1", "0")
Invoke-InteractiveCase "interactive-security-back" @("2", "0", "0")
Invoke-InteractiveCase "interactive-optimization-back" @("3", "0", "0")
Invoke-InteractiveCase "interactive-optimization-performance-cancel" @("3", "3", "n", "0", "0")
Invoke-InteractiveCase "interactive-optimization-aggressive-cancel" @("3", "4", "n", "0", "0")
Invoke-InteractiveCase "interactive-apps-back" @("4", "0", "0")
Invoke-InteractiveCase "interactive-logs-back" @("5", "0", "0")

$results | ConvertTo-Json -Depth 5

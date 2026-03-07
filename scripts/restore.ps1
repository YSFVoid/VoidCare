param()

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Path $PSScriptRoot -Parent
dotnet restore (Join-Path $repoRoot "VoidCare.sln")

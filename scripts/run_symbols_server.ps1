<#
.SYNOPSIS
    Starts the Symbolic LLM Copilot Server for OpenCode / Editor.
.PARAMETER Port
    HTTP port to bind (default: 8099).
.PARAMETER RepoDir
    Root repository path to index for AST and Blast Radius (default: current directory).
#>
param(
    [int]$Port = 8099,
    [string]$RepoDir = "."
)

$Corpora = "data/texts/bible.txt;data/c_lang/c_corpus.txt"
$ExePath = if (Test-Path "build-gcc\symbols-server.exe") { "build-gcc\symbols-server.exe" } elseif (Test-Path "build\symbols-server.exe") { "build\symbols-server.exe" } else { $null }

if (-not $ExePath) {
    Write-Error "symbols-server.exe not found. Please compile with: cmake --build build-gcc --target symbols-server"
    exit 1
}

Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "  STARTING SYMBOLIC LLM LOCAL COPILOT SERVER ON PORT $Port" -ForegroundColor Green
Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "  Repository Graph: $RepoDir"
Write-Host "  Corpora:          $Corpora"
Write-Host "  Commonsense:      data/commonsense.bin"
Write-Host "  Episodic Memory:  data/memory/episodic.tsv"
Write-Host "  Endpoint:         http://127.0.0.1:$Port/v1/chat/completions"
Write-Host "======================================================================`n"

& $ExePath $Port $RepoDir $Corpora

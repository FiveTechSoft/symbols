@echo off
REM ============================================================
REM run_symbols_server.bat: Starts Symbolic LLM Copilot Server
REM Port: 8099 (default)
REM Repo: Current directory (AST & Blast Radius Knowledge Graph)
REM Corpora: Bible text, C Language ontology, Commonsense Snapshot
REM ============================================================

set PORT=%1
if "%PORT%"=="" set PORT=8099

set REPO_DIR=%2
if "%REPO_DIR%"=="" set REPO_DIR=.

set CORPORA=data/texts/bible.txt;data/c_lang/c_corpus.txt

echo ======================================================================
echo   STARTING SYMBOLIC LLM LOCAL COPILOT SERVER ON PORT %PORT%
echo ======================================================================
echo   Repository Graph: %REPO_DIR%
echo   Corpora:          %CORPORA%
echo   Commonsense:      data/commonsense.bin
echo   Episodic Memory:  data/memory/episodic.tsv
echo   Endpoint:         http://127.0.0.1:%PORT%/v1/chat/completions
echo ======================================================================
echo.

if exist build-gcc\symbols-server.exe (
    build-gcc\symbols-server.exe %PORT% %REPO_DIR% %CORPORA%
) else if exist build\symbols-server.exe (
    build\symbols-server.exe %PORT% %REPO_DIR% %CORPORA%
) else (
    echo Error: symbols-server.exe not found in build-gcc\ or build\
    echo Please compile first: cmake --build build-gcc --target symbols-server
    exit /b 1
)

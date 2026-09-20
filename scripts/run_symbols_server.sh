#!/usr/bin/env bash
# ============================================================
# run_symbols_server.sh: Starts Symbolic LLM Copilot Server
# ============================================================

PORT=${1:-8099}
REPO_DIR=${2:-.}
CORPORA="data/c_lang/c_corpus.txt"

if [ -f "build-gcc/symbols-server" ]; then
    EXE="build-gcc/symbols-server"
elif [ -f "build/symbols-server" ]; then
    EXE="build/symbols-server"
elif [ -f "build-gcc/symbols-server.exe" ]; then
    EXE="build-gcc/symbols-server.exe"
else
    echo "Error: symbols-server executable not found. Please compile first."
    exit 1
fi

echo "======================================================================"
echo "  STARTING SYMBOLIC LLM LOCAL COPILOT SERVER ON PORT ${PORT}"
echo "======================================================================"
echo "  Repository Graph: ${REPO_DIR}"
echo "  Corpora:          ${CORPORA}"
echo "  Commonsense:      data/commonsense.bin"
echo "  Episodic Memory:  data/memory/episodic.tsv"
echo "  Endpoint:         http://127.0.0.1:${PORT}/v1/chat/completions"
echo "======================================================================"
echo

exec "$EXE" "$PORT" "$REPO_DIR" "$CORPORA"

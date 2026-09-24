#!/bin/sh
# opencode_agent.sh WORKDIR TASK_FILE: one bank task through the real
# OpenCode client, with symbols-server as its model. For bank_harness.py:
#   python3 tools/bank_harness.py --agent "tools/opencode_agent.sh {workdir} {task_file}"
# Needs: symbols-server listening on 127.0.0.1:8099 and `opencode` on PATH
# (or OPENCODE_BIN). The config lives outside the task's workspace so the
# workspace the operators see is exactly the task's before/ tree.
here=$(cd "$(dirname "$0")/.." && pwd)
bin=${OPENCODE_BIN:-opencode}
cd "$1" || exit 127
OPENCODE_CONFIG="$here/opencode.json" exec timeout "${OPENCODE_TIMEOUT:-90}" "$bin" run "$(cat "$2")"

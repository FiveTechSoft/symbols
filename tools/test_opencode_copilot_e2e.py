#!/usr/bin/env python3
"""
test_opencode_copilot_e2e.py: End-to-end verification of Symbolic LLM
as a local OpenCode copilot service.
"""

import subprocess
import time
import urllib.request
import json
import os
import sys

def main():
    print("======================================================================")
    print("  E2E TEST: SYMBOLIC LLM LOCAL COPILOT FOR OPENCODE / EDITORS         ")
    print("======================================================================")

    port = 8099
    repo_path = os.path.abspath(".")
    corpus_arg = "data/texts/bible.txt;data/c_lang/c_corpus.txt"
    server_exe = os.path.join("build-gcc", "symbols-server.exe")

    if not os.path.exists(server_exe):
        print(f"Error: {server_exe} not found. Please compile first.")
        return 1

    already_running = False
    try:
        req_check = urllib.request.Request(f"http://127.0.0.1:{port}/v1/models")
        with urllib.request.urlopen(req_check, timeout=1) as r:
            if r.status == 200:
                already_running = True
                print(f"\n[1] Detected symbols-server already active on port {port}. Reusing resident instance.")
    except Exception:
        pass

    proc = None
    if not already_running:
        print(f"\n[1] Starting symbols-server on port {port}...")
        print(f"    Repository: {repo_path}")
        print(f"    Corpora:    {corpus_arg}")

        proc = subprocess.Popen(
            [server_exe, str(port), repo_path, corpus_arg],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        time.sleep(2.0)

        # Check if process is running
        if proc.poll() is not None:
            stdout, stderr = proc.communicate()
            print(f"Server failed to start. Return code: {proc.returncode}")
            print(f"Stderr:\n{stderr}")
            return 1

    try:
        base_url = f"http://127.0.0.1:{port}"

        # 1. Test GET /v1/models
        print("\n[2] Testing GET /v1/models...")
        req_models = urllib.request.Request(f"{base_url}/v1/models")
        with urllib.request.urlopen(req_models, timeout=5) as resp:
            data = json.loads(resp.read().decode("utf-8"))
        print(f"    Response: {json.dumps(data)}")
        assert "data" in data, "Expected 'data' array in /v1/models"
        assert len(data["data"]) > 0
        assert data["data"][0]["id"] == "symbols"
        print("    --> PASS: Models endpoint operational.")

        # 2. Test Factual Bible Query
        print("\n[3] Testing Factual Knowledge Query (Father of David)...")
        payload = {
            "model": "symbols",
            "messages": [
                {"role": "user", "content": "Who is the father of David?"}
            ]
        }
        req = urllib.request.Request(
            f"{base_url}/v1/chat/completions",
            data=json.dumps(payload).encode("utf-8"),
            headers={"Content-Type": "application/json"}
        )
        with urllib.request.urlopen(req, timeout=5) as resp:
            res = json.loads(resp.read().decode("utf-8"))
        content = res["choices"][0]["message"]["content"]
        print(f"    Assistant reply: {content}")
        assert "jesse" in content.lower(), f"Expected 'Jesse' in answer, got: {content}"
        print("    --> PASS: Factual knowledge query verified.")

        # 3. Test Commonsense Causal Reasoning (data/commonsense.bin)
        print("\n[4] Testing Commonsense Physical Reasoning Query (Glass drop)...")
        payload_cs = {
            "model": "symbols",
            "messages": [
                {"role": "user", "content": "¿qué pasa si se cae un vaso de cristal al suelo?"}
            ]
        }
        req_cs = urllib.request.Request(
            f"{base_url}/v1/chat/completions",
            data=json.dumps(payload_cs).encode("utf-8"),
            headers={"Content-Type": "application/json"}
        )
        with urllib.request.urlopen(req_cs, timeout=5) as resp:
            res_cs = json.loads(resp.read().decode("utf-8"))
        content_cs = res_cs["choices"][0]["message"]["content"]
        print(f"    Assistant reply: {content_cs}")
        assert "rompera" in content_cs.lower() or "shatter" in content_cs.lower(), f"Expected shatter/rompera in answer, got: {content_cs}"
        print("    --> PASS: Commonsense reasoning verified.")

        # 4. Test OpenCode Agentic Tool Calling (STRIPS Plan)
        print("\n[5] Testing OpenCode Agentic Tool-Calling Workflow...")
        task_payload = {
            "model": "symbols",
            "session_id": "opencode-copilot-test",
            "messages": [
                {"role": "user", "content": "Fix bug in agent_core.c and verify build"}
            ],
            "tools": [
                {"type": "function", "function": {"name": "inspect_code", "description": "Read file lines"}},
                {"type": "function", "function": {"name": "apply_patch", "description": "Apply surgical patch"}},
                {"type": "function", "function": {"name": "execute_command", "description": "Run shell command"}}
            ]
        }
        req_tool = urllib.request.Request(
            f"{base_url}/v1/chat/completions",
            data=json.dumps(task_payload).encode("utf-8"),
            headers={"Content-Type": "application/json"}
        )
        with urllib.request.urlopen(req_tool, timeout=5) as resp:
            res_tool = json.loads(resp.read().decode("utf-8"))
        choice = res_tool["choices"][0]
        assert choice["finish_reason"] == "tool_calls", f"Expected tool_calls, got {choice['finish_reason']}"
        tool_call = choice["message"]["tool_calls"][0]
        print(f"    Tool call emitted: {tool_call['function']['name']} (args: {tool_call['function']['arguments']})")
        print("    --> PASS: OpenCode tool calling verified.")

        print("\n======================================================================")
        print("  ALL COPILOT INTEGRATION VERIFICATIONS PASSED (100% SUCCESS)         ")
        print("======================================================================")
        return 0

    finally:
        if proc is not None:
            proc.terminate()
            try:
                proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                proc.kill()
            print("\n[6] symbols-server process terminated cleanly.")
        else:
            print("\n[6] Keeping resident symbols-server process active.")

if __name__ == "__main__":
    sys.exit(main())

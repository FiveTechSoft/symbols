import urllib.request
import json
import time
import os
import subprocess

URL = "http://127.0.0.1:8099/v1/chat/completions"

OPENCODE_TOOLS = [
    {"type": "function", "function": {"name": "bash", "description": "Execute command"}},
    {"type": "function", "function": {"name": "edit", "description": "Edit file"}},
    {"type": "function", "function": {"name": "glob", "description": "Find files"}},
    {"type": "function", "function": {"name": "grep", "description": "Search code"}},
    {"type": "function", "function": {"name": "read", "description": "Read file"}},
    {"type": "function", "function": {"name": "write", "description": "Write file"}},
]

def query(payload):
    req = urllib.request.Request(
        URL,
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"}
    )
    with urllib.request.urlopen(req, timeout=10) as r:
        return json.loads(r.read().decode("utf-8"))

def test_file_creation_flow():
    print("\n--- Test 1: 'crea un fichero test.txt' Flow ---")
    payload = {
        "model": "symbols",
        "tools": OPENCODE_TOOLS,
        "messages": [
            {"role": "user", "content": "crea un fichero test.txt"}
        ]
    }
    res = query(payload)
    choice = res["choices"][0]
    fr = choice.get("finish_reason")
    tc = choice.get("message", {}).get("tool_calls", [])
    assert fr == "tool_calls", f"Expected tool_calls, got {fr}"
    assert len(tc) > 0, "Expected at least 1 tool call"
    tool_name = tc[0]["function"]["name"]
    tool_args = json.loads(tc[0]["function"]["arguments"])
    print(f"  [PASS] Step 1 dispatched tool: {tool_name}, args: {tool_args}")
    assert tool_name in ["write", "bash", "edit"], f"Unexpected tool: {tool_name}"
    target = tool_args.get("filePath") or tool_args.get("command")
    assert "test.txt" in target, f"Expected 'test.txt' in arguments, got {target}"

    # Step 2: Client returns tool execution success
    tool_call_id = tc[0]["id"]
    payload["messages"].append(choice["message"])
    payload["messages"].append({
        "role": "tool",
        "tool_call_id": tool_call_id,
        "name": tool_name,
        "content": "{\"status\": \"ok\", \"bytes_written\": 0}"
    })
    res2 = query(payload)
    choice2 = res2["choices"][0]
    fr2 = choice2.get("finish_reason")
    content2 = choice2.get("message", {}).get("content", "")
    print(f"  [PASS] Step 2 completion finish_reason: {fr2}")
    print(f"  [PASS] Summary message: {content2.splitlines()[0] if content2 else ''}")
    assert fr2 == "stop", f"Expected stop, got {fr2}"
    assert "test.txt" in content2, f"Expected 'test.txt' in summary, got: {content2}"
    assert "creado" in content2.lower() or "creación" in content2.lower(), f"Expected created confirmation: {content2}"

def test_subfolder_inspection():
    print("\n--- Test 2: 'lista las subcarpetas' ---")
    payload = {
        "model": "symbols",
        "tools": OPENCODE_TOOLS,
        "messages": [
            {"role": "user", "content": "lista las subcarpetas"}
        ]
    }
    res = query(payload)
    choice = res["choices"][0]
    fr = choice.get("finish_reason")
    tc = choice.get("message", {}).get("tool_calls", [])
    assert fr == "tool_calls", f"Expected tool_calls, got {fr}"
    assert len(tc) > 0, "Expected at least 1 tool call"
    tool_name = tc[0]["function"]["name"]
    tool_args = json.loads(tc[0]["function"]["arguments"])
    print(f"  [PASS] Dispatched tool: {tool_name}, args: {tool_args}")
    assert tool_name == "glob", f"Expected 'glob', got {tool_name}"

def test_large_body_payload():
    print("\n--- Test 3: Large Multi-Turn Body (>75 KB) ---")
    # Generate large mock history with 80 KB of tool output
    large_text = "src/code_graph.c: line of code with symbols and tokens\n" * 1500
    payload = {
        "model": "symbols",
        "tools": OPENCODE_TOOLS,
        "messages": [
            {"role": "user", "content": "inspect codebase"},
            {"role": "assistant", "tool_calls": [{"id": "call_mock", "type": "function", "function": {"name": "read", "arguments": "{\"filePath\":\"large.txt\"}"}}]},
            {"role": "tool", "tool_call_id": "call_mock", "name": "read", "content": large_text},
            {"role": "user", "content": "lista las subcarpetas"}
        ]
    }
    raw_bytes = len(json.dumps(payload).encode("utf-8"))
    print(f"  Sending payload of {raw_bytes} bytes ({raw_bytes / 1024:.1f} KB)...")
    res = query(payload)
    choice = res["choices"][0]
    fr = choice.get("finish_reason")
    print(f"  [PASS] Server accepted {raw_bytes / 1024:.1f} KB payload and returned finish_reason: {fr}")
    assert fr in ["tool_calls", "stop"], f"Unexpected finish_reason: {fr}"

def test_fibonacci_synthesis():
    print("\n--- Test 4: 'escribe en C la funcion de fibonacci' ---")
    payload = {
        "model": "symbols",
        "tools": OPENCODE_TOOLS,
        "messages": [
            {"role": "user", "content": "escribe en C la funcion de fibonacci"}
        ]
    }
    res = query(payload)
    choice = res["choices"][0]
    fr = choice.get("finish_reason")
    content = choice.get("message", {}).get("content", "")
    print(f"  [PASS] finish_reason: {fr}")
    print(f"  [PASS] Snippet: {content.splitlines()[0] if content else ''}")
    assert fr == "stop", f"Expected stop, got {fr}"
    assert "fibonacci" in content.lower(), f"Expected fibonacci in content: {content}"
    assert "uint64_t" in content or "int" in content, f"Expected C code in content: {content}"
    assert "```c" in content, f"Expected C markdown block in content: {content}"

def test_fibinacci_typo_synthesis():
    print("\n--- Test 4b: 'funcion fibinacci en C' (Typo & Noun-Only Phrase) ---")
    payload = {
        "model": "symbols",
        "tools": OPENCODE_TOOLS,
        "messages": [
            {"role": "user", "content": "funcion fibinacci en C"}
        ]
    }
    res = query(payload)
    choice = res["choices"][0]
    fr = choice.get("finish_reason")
    tc = choice.get("message", {}).get("tool_calls", [])
    content = choice.get("message", {}).get("content", "")
    print(f"  [PASS] finish_reason: {fr}")
    assert fr == "stop", f"Expected stop, got {fr}"
    assert len(tc) == 0, f"Expected 0 tool calls for code synthesis, got {len(tc)}"
    assert "fibonacci" in content.lower() or "fibinacci" in content.lower()
    assert "uint64_t" in content or "int" in content
    assert "```c" in content

def test_quicksort_synthesis():
    print("\n--- Test 4c: 'quicksort en C' Synthesis ---")
    payload = {
        "model": "symbols",
        "tools": OPENCODE_TOOLS,
        "messages": [
            {"role": "user", "content": "quicksort en C"}
        ]
    }
    res = query(payload)
    choice = res["choices"][0]
    fr = choice.get("finish_reason")
    tc = choice.get("message", {}).get("tool_calls", [])
    content = choice.get("message", {}).get("content", "")
    print(f"  [PASS] finish_reason: {fr}")
    assert fr == "stop"
    assert len(tc) == 0
    assert "quicksort" in content.lower()
    assert "partition" in content.lower() or "swap" in content.lower()
    assert "```c" in content

def test_linked_list_synthesis():
    print("\n--- Test 4d: 'lista enlazada en C' Synthesis ---")
    payload = {
        "model": "symbols",
        "tools": OPENCODE_TOOLS,
        "messages": [
            {"role": "user", "content": "lista enlazada en C"}
        ]
    }
    res = query(payload)
    choice = res["choices"][0]
    fr = choice.get("finish_reason")
    tc = choice.get("message", {}).get("tool_calls", [])
    content = choice.get("message", {}).get("content", "")
    print(f"  [PASS] finish_reason: {fr}")
    assert fr == "stop"
    assert len(tc) == 0
    assert "node" in content.lower()
    assert "next" in content.lower()
    assert "```c" in content

def test_read_file_synthesis():
    print("\n--- Test 4e: 'leer archivo en C' Synthesis ---")
    payload = {
        "model": "symbols",
        "tools": OPENCODE_TOOLS,
        "messages": [
            {"role": "user", "content": "leer archivo en C"}
        ]
    }
    res = query(payload)
    choice = res["choices"][0]
    fr = choice.get("finish_reason")
    tc = choice.get("message", {}).get("tool_calls", [])
    content = choice.get("message", {}).get("content", "")
    print(f"  [PASS] finish_reason: {fr}")
    assert fr == "stop"
    assert len(tc) == 0
    assert "fopen" in content.lower()
    assert "fgets" in content.lower()
    assert "```c" in content

def test_greeting_natural():
    print("\n--- Test 5: 'hola' Natural Greeting ---")
    payload = {
        "model": "symbols",
        "tools": OPENCODE_TOOLS,
        "messages": [
            {"role": "user", "content": "hola"}
        ]
    }
    res = query(payload)
    choice = res["choices"][0]
    fr = choice.get("finish_reason")
    content = choice.get("message", {}).get("content", "")
    print(f"  [PASS] finish_reason: {fr}")
    print(f"  [PASS] Reply: {content}")
    assert fr == "stop", f"Expected stop, got {fr}"
    assert "hold" not in content.lower() and "behold" not in content.lower(), f"Unexpected bible text in greeting: {content}"
    assert any(g in content.lower() for g in ["hola", "saludos", "hello", "ayud"]), f"Expected greeting response: {content}"

def test_dir_wildcard_flow():
    print("\n--- Test 6: 'dir *.*' Wildcard Inspection Flow ---")
    payload = {
        "model": "symbols",
        "tools": OPENCODE_TOOLS,
        "messages": [
            {"role": "user", "content": "dir *.*"}
        ]
    }
    res = query(payload)
    choice = res["choices"][0]
    fr = choice.get("finish_reason")
    tc = choice.get("message", {}).get("tool_calls", [])
    assert fr == "tool_calls", f"Expected tool_calls, got {fr}"
    assert len(tc) > 0, "Expected at least 1 tool call"
    tool_name = tc[0]["function"]["name"]
    tool_args = json.loads(tc[0]["function"]["arguments"])
    print(f"  [PASS] Step 1 dispatched tool: {tool_name}, args: {tool_args}")
    assert tool_name == "glob", f"Expected 'glob', got {tool_name}"
    assert tool_args.get("pattern") in ["*.*", "*"], f"Expected '*.*' pattern, got {tool_args.get('pattern')}"

    # Step 2: Client returns OpenCode Glob JSON response with matches and "error": null
    tool_call_id = tc[0]["id"]
    payload["messages"].append(choice["message"])
    payload["messages"].append({
        "role": "tool",
        "tool_call_id": tool_call_id,
        "name": tool_name,
        "content": json.dumps({
            "status": "ok",
            "matches": ["build-gcc", "src", "include", "CMakeLists.txt"],
            "error": None
        })
    })
    res2 = query(payload)
    choice2 = res2["choices"][0]
    fr2 = choice2.get("finish_reason")
    content2 = choice2.get("message", {}).get("content", "")
    print(f"  [PASS] Step 2 completion finish_reason: {fr2}")
    print(f"  [PASS] Clean content report:\n{content2}")
    assert fr2 == "stop", f"Expected finish_reason == 'stop', got {fr2}"
    assert "FAILED" not in content2, f"Did not expect FAILED in output: {content2}"
    assert "Verification Failed" not in content2, f"Did not expect verification failed: {content2}"
    assert "build-gcc" in content2, f"Expected 'build-gcc' in output: {content2}"
    assert "CMakeLists.txt" in content2, f"Expected 'CMakeLists.txt' in output: {content2}"
    assert "Exploracion completada con exito" in content2 or "Revision de Directorio" in content2

def test_dir_dot_flow():
    print("\n--- Test 7: 'dir .' Flow with Array Tool Response & Omitted Name ---")
    payload = {
        "model": "symbols",
        "tools": OPENCODE_TOOLS,
        "messages": [
            {"role": "user", "content": "dir ."}
        ]
    }
    res = query(payload)
    choice = res["choices"][0]
    fr = choice.get("finish_reason")
    tc = choice.get("message", {}).get("tool_calls", [])
    assert fr == "tool_calls", f"Expected tool_calls, got {fr}"
    tool_call_id = tc[0]["id"]

    # Step 2: Multi-part array response with name omitted
    payload["messages"].append(choice["message"])
    payload["messages"].append({
        "role": "tool",
        "tool_call_id": tool_call_id,
        "content": [
            {"type": "text", "text": "build-gcc\nsrc\ninclude\nREADME.md"}
        ]
    })
    res2 = query(payload)
    choice2 = res2["choices"][0]
    fr2 = choice2.get("finish_reason")
    content2 = choice2.get("message", {}).get("content", "")
    print(f"  [PASS] Step 2 completion finish_reason: {fr2}")
    assert fr2 == "stop", f"Expected finish_reason == 'stop', got {fr2}"
    assert "FAILED" not in content2
    assert "build-gcc" in content2
    assert "src" in content2

if __name__ == "__main__":
    port = 8099
    repo_path = os.path.abspath(".")
    corpus_arg = "data/c_lang/c_corpus.txt"
    server_exe = os.path.join("build-gcc", "symbols-server.exe")

    already_running = False
    try:
        req_check = urllib.request.Request(f"http://127.0.0.1:{port}/v1/models")
        with urllib.request.urlopen(req_check, timeout=1) as r:
            if r.status == 200:
                already_running = True
                print(f"[Init] Detected symbols-server already active on port {port}. Reusing resident instance.")
    except Exception:
        pass

    proc = None
    if not already_running:
        if not os.path.exists(server_exe):
            print(f"Error: {server_exe} not found. Please compile first.")
            exit(1)
        print(f"[Init] Starting symbols-server on port {port}...")
        proc = subprocess.Popen(
            [server_exe, str(port), repo_path, corpus_arg],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        time.sleep(2.0)
        if proc.poll() is not None:
            _, stderr = proc.communicate()
            print(f"Server failed to start. Return code: {proc.returncode}\nStderr:\n{stderr}")
            exit(1)

    try:
        test_file_creation_flow()
        test_subfolder_inspection()
        test_large_body_payload()
        test_fibonacci_synthesis()
        test_fibinacci_typo_synthesis()
        test_quicksort_synthesis()
        test_linked_list_synthesis()
        test_read_file_synthesis()
        test_greeting_natural()
        test_dir_wildcard_flow()
        test_dir_dot_flow()
        print("\n" + "=" * 60)
        print("  ALL USER SCENARIOS VERIFIED END-TO-END (100% PASS)")
        print("=" * 60)
    finally:
        if proc is not None:
            proc.terminate()
            proc.wait(timeout=5)
            print("\n[Cleanup] Stopped symbols-server instance.")



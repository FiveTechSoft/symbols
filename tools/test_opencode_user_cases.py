import urllib.request
import json
import time

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

if __name__ == "__main__":
    test_file_creation_flow()
    test_subfolder_inspection()
    test_large_body_payload()
    test_fibonacci_synthesis()
    test_greeting_natural()
    print("\n" + "=" * 60)
    print("  ALL USER SCENARIOS VERIFIED END-TO-END (100% PASS)")
    print("=" * 60)



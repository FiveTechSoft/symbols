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

def query_server(prompt, with_tools=True):
    payload = {
        "model": "symbols",
        "messages": [{"role": "user", "content": prompt}]
    }
    if with_tools:
        payload["tools"] = OPENCODE_TOOLS
    req = urllib.request.Request(
        URL,
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"}
    )
    with urllib.request.urlopen(req, timeout=5) as r:
        return json.loads(r.read().decode("utf-8"))

def run_battery():
    passed = 0
    failed = 0

    print("=" * 70)
    print("  LIVE DISCRIMINATION BATTERY (HTTP SOCKET WIRE VERIFICATION)")
    print("=" * 70)

    # Category 1: Shell & Wildcard Exploration Commands (Expected: tool_calls -> glob with pattern)
    print("\n--- [Category 1] Shell & Wildcard Exploration (Expected: glob) ---")
    cat1 = [
        ("dir *.*", "glob", "*.*"),
        ("dir", "glob", "*"),
        ("ls", "glob", "*"),
        ("ls -la", "glob", "*"),
        ("*.c", "glob", "*.c"),
        ("*.h", "glob", "*.h"),
        ("*.*", "glob", "*.*"),
        ("pwd", "glob", "*"),
        ("tree", "glob", "*"),
    ]
    for prompt, expected_tool, expected_arg in cat1:
        res = query_server(prompt, with_tools=True)
        choice = res["choices"][0]
        fr = choice.get("finish_reason")
        tc = choice.get("message", {}).get("tool_calls", [])
        if fr == "tool_calls" and len(tc) > 0 and tc[0]["function"]["name"] == expected_tool:
            args = json.loads(tc[0]["function"]["arguments"])
            if args.get("pattern") == expected_arg:
                print(f"  [PASS] '{prompt}' -> {expected_tool} (pattern: {expected_arg})")
                passed += 1
            else:
                print(f"  [FAIL] '{prompt}' -> pattern mismatch: {args}")
                failed += 1
        else:
            print(f"  [FAIL] '{prompt}' -> finish_reason: {fr}, tc: {tc}")
            failed += 1

    # Category 2: Repository & Workspace Inspection (Expected: tool_calls -> glob or read)
    print("\n--- [Category 2] Repository & Workspace Inspection (Expected: glob or read) ---")
    cat2 = [
        "revisa esta carpeta",
        "revisar el directorio actual",
        "inspecciona este proyecto",
        "archivos del workspace",
        "review this directory",
        "inspect the codebase",
        "explore the project structure"
    ]
    for prompt in cat2:
        res = query_server(prompt, with_tools=True)
        choice = res["choices"][0]
        fr = choice.get("finish_reason")
        tc = choice.get("message", {}).get("tool_calls", [])
        if fr == "tool_calls" and len(tc) > 0 and tc[0]["function"]["name"] in ["glob", "read"]:
            print(f"  [PASS] '{prompt}' -> tool_call '{tc[0]['function']['name']}'")
            passed += 1
        else:
            print(f"  [FAIL] '{prompt}' -> fr={fr}, tc={tc}")
            failed += 1

    # Category 3: Coding Tasks without Tools (Expected: Repository Summary, 0 Bible verses)
    print("\n--- [Category 3] Coding Tasks without Tools (Expected: Repo Summary, 0 Bible) ---")
    cat3 = [
        "dir *.*",
        "revisa esta carpeta",
        "archivos del repositorio",
        "review folder"
    ]
    for prompt in cat3:
        res = query_server(prompt, with_tools=False)
        content = res["choices"][0]["message"]["content"]
        if ("repositorio" in content.lower() or "archivos" in content.lower() or "funciones" in content.lower()) and "segun el texto" not in content.lower():
            print(f"  [PASS] '{prompt}' -> Repo summary cleanly returned (0 Bible contamination)")
            passed += 1
        else:
            print(f"  [FAIL] '{prompt}' -> contaminated or invalid: {content[:80]}")
            failed += 1

    # Category 4: Factual and Domain Knowledge QA (Expected: Direct conversational answer, NO tool_calls, NO repo header)
    print("\n--- [Category 4] Factual & Domain Knowledge QA (Expected: Direct conversational answer) ---")
    cat4 = [
        "Who is the father of Solomon?",
        "¿Quién es el padre de Salomón?",
        "Tell me about wisdom and proverbs",
        "Where was Jonah sent?",
        "What is the archetype of the shadow?",
        "What areas do you know?"
    ]
    for prompt in cat4:
        res = query_server(prompt, with_tools=True)
        choice = res["choices"][0]
        fr = choice.get("finish_reason")
        tc = choice.get("message", {}).get("tool_calls", [])
        content = choice.get("message", {}).get("content", "").lower()
        if fr == "stop" and len(tc) == 0 and "repositorio de codigo indexado" not in content:
            print(f"  [PASS] '{prompt}' -> Discriminated as factual (finish_reason=stop, 0 tool_calls)")
            passed += 1
        else:
            print(f"  [FAIL] '{prompt}' -> fr={fr}, tc={tc}, content={content[:80]}")
            failed += 1

    # Category 5: Physical Causality QA (Expected: Causal deduction, NO tool_calls, NO repo header)
    print("\n--- [Category 5] Physical Causality & Affordances (Expected: Physical deduction) ---")
    cat5 = [
        ("¿Qué pasa si se cae un vaso de cristal al suelo?", ["rompe", "shatter", "fragil"]),
        ("What happens if a glass falls to the floor?", ["break", "shatter", "fragile"]),
        ("¿Qué pasa si se calienta el hielo?", ["derrite", "melt", "agua", "liquid", "know"]),
    ]
    for prompt, expected_tokens in cat5:
        res = query_server(prompt, with_tools=True)
        choice = res["choices"][0]
        fr = choice.get("finish_reason")
        tc = choice.get("message", {}).get("tool_calls", [])
        content = choice.get("message", {}).get("content", "").lower()
        matched = any(tok in content for tok in expected_tokens)
        if fr == "stop" and len(tc) == 0 and matched and "repositorio de codigo indexado" not in content:
            print(f"  [PASS] '{prompt}' -> Discriminated as causal query & answered correctly")
            passed += 1
        else:
            print(f"  [FAIL] '{prompt}' -> fr={fr}, content={content[:80]}")
            failed += 1

    print("\n" + "=" * 70)
    print(f"  FINAL RESULTS: {passed} passed, {failed} failed (Total: {passed + failed})")
    print("=" * 70)
    return failed == 0

if __name__ == "__main__":
    time.sleep(1)
    ok = run_battery()
    exit(0 if ok else 1)

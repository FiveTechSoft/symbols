import subprocess
import time
import urllib.request
import json
import os
import sys

def test_opencode_agentic_server():
    print("======================================================================")
    print("  TEST: OPENCODE AGENTIC TOOL CALLING & MULTI-TURN REACt OVER HTTP   ")
    print("======================================================================")
    
    port = 8095
    base_url = f"http://127.0.0.1:{port}/v1/chat/completions"
    
    # Launch symbols-server
    server_proc = subprocess.Popen(
        ["build-gcc/symbols-server.exe", str(port), "data/texts/bible.txt"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    time.sleep(1.5)
    
    try:
        # Step 1: OpenCode sends a coding task with declared tools
        print("\n--- Step 1: OpenCode initiates coding task with tools declared ---")
        messages = [
            {"role": "user", "content": "Fix bug in main.c and verify build"}
        ]
        tools_decl = [
            {"type": "function", "function": {"name": "apply_patch", "description": "Apply patch"}},
            {"type": "function", "function": {"name": "execute_command", "description": "Run bash command"}},
            {"type": "function", "function": {"name": "inspect_code", "description": "Read file lines"}}
        ]
        
        req_payload = {
            "model": "symbols",
            "session_id": "opencode-session-1",
            "messages": messages,
            "tools": tools_decl
        }
        
        req = urllib.request.Request(
            base_url,
            data=json.dumps(req_payload).encode("utf-8"),
            headers={"Content-Type": "application/json"}
        )
        with urllib.request.urlopen(req, timeout=5) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            
        choice = data["choices"][0]
        finish_reason = choice["finish_reason"]
        print(f"Server response finish_reason: {finish_reason}")
        assert finish_reason == "tool_calls", f"Expected tool_calls, got {finish_reason}"
        
        tool_calls = choice["message"]["tool_calls"]
        assert len(tool_calls) > 0, "Expected at least 1 tool call"
        first_call = tool_calls[0]
        print(f"Tool call emitted by Symbolic LLM: ID={first_call['id']} Function={first_call['function']['name']} Args={first_call['function']['arguments']}")
        
        # Step 2: OpenCode executes the tool and sends the observation back
        print("\n--- Step 2: OpenCode simulates tool execution and sends output ---")
        messages.append(choice["message"])
        messages.append({
            "role": "tool",
            "tool_call_id": first_call["id"],
            "name": first_call["function"]["name"],
            "content": json.dumps({"status": "success", "file": "src/main.c", "lines": 100})
        })
        
        req_payload["messages"] = messages
        req = urllib.request.Request(
            base_url,
            data=json.dumps(req_payload).encode("utf-8"),
            headers={"Content-Type": "application/json"}
        )
        with urllib.request.urlopen(req, timeout=5) as resp:
            data2 = json.loads(resp.read().decode("utf-8"))
            
        choice2 = data2["choices"][0]
        print(f"Second turn finish_reason: {choice2['finish_reason']}")
        
        # Continue the plan until finish_reason == 'stop'
        turn = 2
        while choice2["finish_reason"] == "tool_calls" and turn < 8:
            turn += 1
            tc = choice2["message"]["tool_calls"][0]
            print(f"Turn {turn} Tool Call: {tc['function']['name']}")
            messages.append(choice2["message"])
            messages.append({
                "role": "tool",
                "tool_call_id": tc["id"],
                "name": tc["function"]["name"],
                "content": json.dumps({"status": "ok", "exit_code": 0})
            })
            req_payload["messages"] = messages
            req = urllib.request.Request(
                base_url,
                data=json.dumps(req_payload).encode("utf-8"),
                headers={"Content-Type": "application/json"}
            )
            with urllib.request.urlopen(req, timeout=5) as resp:
                data2 = json.loads(resp.read().decode("utf-8"))
            choice2 = data2["choices"][0]

        print(f"\nFinal completion finish_reason: {choice2['finish_reason']}")
        assert choice2["finish_reason"] == "stop", "Expected final finish_reason == 'stop'"
        final_text = choice2["message"]["content"]
        print(f"Final Report Emitted by Server:\n{final_text}")
        assert "Autonomous Coding Task Completed" in final_text or "Verified" in final_text

        # Step 3: Factual non-coding query still works seamlessly
        print("\n--- Step 3: Factual query without coding intent returns direct answer ---")
        fact_payload = {
            "model": "symbols",
            "messages": [{"role": "user", "content": "quien es el padre de david"}]
        }
        req = urllib.request.Request(
            base_url,
            data=json.dumps(fact_payload).encode("utf-8"),
            headers={"Content-Type": "application/json"}
        )
        with urllib.request.urlopen(req, timeout=5) as resp:
            fact_data = json.loads(resp.read().decode("utf-8"))
        fact_choice = fact_data["choices"][0]
        print(f"Factual query finish_reason: {fact_choice['finish_reason']}")
        print(f"Factual answer: {fact_choice['message']['content']}")
        assert fact_choice["finish_reason"] == "stop"
        assert "Jesse" in fact_choice["message"]["content"]

        print("\n======================================================================")
        print("  ALL OPENCODE TOOL CALLING AGENTIC INTEGRATION TESTS PASSED!          ")
        print("======================================================================")
        
    finally:
        server_proc.terminate()
        server_proc.wait(timeout=3)

if __name__ == "__main__":
    test_opencode_agentic_server()

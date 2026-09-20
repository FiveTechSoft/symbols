import subprocess
import time
import urllib.request
import json
import os
import sys

def create_valid_model(filepath):
    import struct
    with open(filepath, "wb") as f:
        magic = 0x53594D42
        version = 5
        sym_count = 3
        rel_count = 1
        emb_count = 0
        emb_dim = 32
        f.write(struct.pack("<IIIIII", magic, version, sym_count, rel_count, emb_count, emb_dim))
        # Symbol 1: DAVID
        f.write(struct.pack("<II5sQ", 1, 5, b"DAVID", 1))
        # Symbol 2: PADRE_DE
        f.write(struct.pack("<II8sQ", 2, 8, b"PADRE_DE", 1))
        # Symbol 3: JESSE
        f.write(struct.pack("<II5sQ", 3, 5, b"JESSE", 1))
        # Relation 1: JESSE PADRE_DE DAVID
        f.write(struct.pack("<IIIQfII", 3, 2, 1, 1, 1.0, 0, 0))
        # Numerics count: 0
        f.write(struct.pack("<I", 0))

def test_server_failure_and_model():
    print("======================================================================")
    print("  TEST: SERVER ERROR HANDLING, FAIL-CLOSED REPORTING & BINARY MODEL   ")
    print("======================================================================")
    
    port = 8096
    base_url = f"http://127.0.0.1:{port}"
    
    bin_path = "build-gcc/test_model.bin"
    create_valid_model(bin_path)

    # 1. Start symbols-server with binary model build-gcc/test_model.bin
    print(f"\n--- 1. Launch symbols-server with binary model {bin_path} ---")
    server_proc = subprocess.Popen(
        ["build-gcc/symbols-server.exe", str(port), bin_path],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    time.sleep(1.5)
    
    try:
        # Check /v1/model/load endpoint
        print(f"\n--- 2. Test /v1/model/load endpoint with {bin_path} ---")
        load_req = urllib.request.Request(
            f"{base_url}/v1/model/load",
            data=json.dumps({"path": bin_path}).encode("utf-8"),
            headers={"Content-Type": "application/json"}
        )
        try:
            with urllib.request.urlopen(load_req, timeout=5) as resp:
                load_data = json.loads(resp.read().decode("utf-8"))
        except Exception as e:
            time.sleep(0.2)
            server_proc.terminate()
            out, err = server_proc.communicate()
            print("SERVER STDERR:\n", err.decode("utf-8", errors="replace"))
            raise e
        print(f"Model Load response: {load_data}")
        assert load_data["status"] == "ok"
        assert load_data["format"] == "binary_v2"
        assert load_data["symbols"] > 0
        assert load_data["relations"] > 0

        # Check /v1/model/save endpoint
        print("\n--- 3. Test /v1/model/save endpoint ---")
        save_path = "build-gcc/test_saved_model.bin"
        if os.path.exists(save_path):
            os.remove(save_path)
        save_req = urllib.request.Request(
            f"{base_url}/v1/model/save",
            data=json.dumps({"path": save_path}).encode("utf-8"),
            headers={"Content-Type": "application/json"}
        )
        with urllib.request.urlopen(save_req, timeout=5) as resp:
            save_data = json.loads(resp.read().decode("utf-8"))
        print(f"Model Save response: {save_data}")
        assert save_data["status"] == "ok"
        assert os.path.exists(save_path), "Saved binary model file exists on disk"

        # Check conversational /save and /load commands
        print("\n--- 4. Test conversational /save and /load commands ---")
        chat_url = f"{base_url}/v1/chat/completions"
        conv_save_payload = {
            "model": "symbols",
            "messages": [{"role": "user", "content": f"/save {save_path}"}]
        }
        req = urllib.request.Request(
            chat_url,
            data=json.dumps(conv_save_payload).encode("utf-8"),
            headers={"Content-Type": "application/json"}
        )
        with urllib.request.urlopen(req, timeout=5) as resp:
            conv_data = json.loads(resp.read().decode("utf-8"))
        print(f"Conversational /save reply: {conv_data['choices'][0]['message']['content']}")
        assert "Model saved to" in conv_data["choices"][0]["message"]["content"]

        # Check tool execution failure handling (fail-closed verification)
        print("\n--- 5. Test Tool Failure Handling: Verify Server NEVER says PASS on error ---")
        coding_payload = {
            "model": "symbols",
            "session_id": "fail-test-session",
            "messages": [{"role": "user", "content": "Fix bug in main.c and build"}],
            "tools": [{"type": "function", "function": {"name": "execute_command", "description": "Run command"}}]
        }
        req = urllib.request.Request(
            chat_url,
            data=json.dumps(coding_payload).encode("utf-8"),
            headers={"Content-Type": "application/json"}
        )
        with urllib.request.urlopen(req, timeout=5) as resp:
            start_data = json.loads(resp.read().decode("utf-8"))
        
        tc = start_data["choices"][0]["message"]["tool_calls"][0]
        print(f"Initiated tool call: {tc['function']['name']}")

        # Simulate tool failure: return non-zero exit code and compiler error
        fail_messages = [
            {"role": "user", "content": "Fix bug in main.c and build"},
            start_data["choices"][0]["message"],
            {
                "role": "tool",
                "tool_call_id": tc["id"],
                "name": tc["function"]["name"],
                "content": json.dumps({
                    "exit_code": 1,
                    "stderr": "src/main.c:12:3: error: unknown type name 'MyType'\nmake: *** Error 1"
                })
            }
        ]

        coding_payload["messages"] = fail_messages
        req = urllib.request.Request(
            chat_url,
            data=json.dumps(coding_payload).encode("utf-8"),
            headers={"Content-Type": "application/json"}
        )
        with urllib.request.urlopen(req, timeout=5) as resp:
            step2_data = json.loads(resp.read().decode("utf-8"))
        
        choice2 = step2_data["choices"][0]
        # Server replanned or emitted another step or final report
        turn = 2
        while choice2["finish_reason"] == "tool_calls" and turn < 8:
            turn += 1
            tc2 = choice2["message"]["tool_calls"][0]
            fail_messages.append(choice2["message"])
            fail_messages.append({
                "role": "tool",
                "tool_call_id": tc2["id"],
                "name": tc2["function"]["name"],
                "content": json.dumps({"exit_code": 2, "stderr": "Build verification failed: error persistent"})
            })
            coding_payload["messages"] = fail_messages
            req = urllib.request.Request(
                chat_url,
                data=json.dumps(coding_payload).encode("utf-8"),
                headers={"Content-Type": "application/json"}
            )
            with urllib.request.urlopen(req, timeout=5) as resp:
                step2_data = json.loads(resp.read().decode("utf-8"))
            choice2 = step2_data["choices"][0]

        final_content = choice2["message"]["content"]
        print(f"Final Report after failed tool calls:\n{final_content}")
        assert "100% Verified" not in final_content, "CRITICAL: Server must NEVER claim '100% Verified' when tools failed!"
        assert "Build: PASS" not in final_content, "CRITICAL: Server must NEVER claim 'Build: PASS' when tools failed!"
        assert "FAILED" in final_content or "FAIL" in final_content, "Server must report FAILED status"
        print("\n>>> CONFIRMED: Fail-closed verification is active. Zero blind PASS emitted!")

        print("\n======================================================================")
        print("  ALL VERIFICATION AND MODEL INTEGRATION TESTS PASSED!               ")
        print("======================================================================")

    finally:
        server_proc.terminate()
        server_proc.wait(timeout=3)

if __name__ == "__main__":
    test_server_failure_and_model()

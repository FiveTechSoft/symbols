import subprocess
import time
import urllib.request
import json
import os

def run_server_test():
    env = os.environ.copy()
    env["SYMBOLS_CORPUS"] = "data/texts/bible.txt;data/texts/jung.txt"
    
    server_proc = subprocess.Popen(
        ["build-gcc/symbols-server.exe", "8089"],
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    def ask(msg, timeout=10):
        req_data = json.dumps({
            "model": "symbols",
            "messages": [{"role": "user", "content": msg}]
        }).encode("utf-8")
        req = urllib.request.Request(
            "http://127.0.0.1:8089/v1/chat/completions",
            data=req_data,
            headers={"Content-Type": "application/json"}
        )
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            res = json.loads(resp.read().decode("utf-8"))
            return res["choices"][0]["message"]["content"].strip()

    # Wait up to 10 seconds for server to be responsive
    for _ in range(20):
        time.sleep(0.5)
        try:
            ask("ping", timeout=1)
            break
        except Exception:
            pass

    try:

        # Bible query
        ans1 = ask("quien es el padre de david")
        print("Q1 (Bible): quien es el padre de david ->", ans1)
        assert "Jesse" in ans1

        # Jung corpus query
        ans2 = ask("quien es Jung?")
        print("Q2 (Jung): quien es Jung? ->", ans2)
        assert "Jung" in ans2

        # Conversational topics prompt
        ans3 = ask("dime las areas que conoces")
        print("Q3 (Topics): dime las areas que conoces ->", ans3)
        assert "abarcan temas como" in ans3 or "areas" in ans3.lower() or "topics" in ans3.lower()

        # Conversational start prompt
        ans4 = ask("inicia una conversacion")
        print("Q4 (Conversation): inicia una conversacion ->", ans4)
        assert "Podemos hablar sobre" in ans4 or "hablar" in ans4.lower()

        print("ALL MULTICORPUS SERVER CHECKS PASSED!")
    finally:
        server_proc.terminate()
        server_proc.wait(timeout=3)

if __name__ == "__main__":
    run_server_test()

import subprocess
import time
import urllib.request
import json
import os

def run_server_test():
    env = os.environ.copy()
    env["SYMBOLS_CORPUS"] = "data/bible/bible_relations.tsv;data/samples/geo_knowledge.tsv"
    
    server_proc = subprocess.Popen(
        ["build-gcc/symbols-server.exe", "8089"],
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    time.sleep(1.5)
    try:
        def ask(msg):
            req_data = json.dumps({
                "model": "symbols",
                "messages": [{"role": "user", "content": msg}]
            }).encode("utf-8")
            req = urllib.request.Request(
                "http://127.0.0.1:8089/v1/chat/completions",
                data=req_data,
                headers={"Content-Type": "application/json"}
            )
            with urllib.request.urlopen(req, timeout=5) as resp:
                res = json.loads(resp.read().decode("utf-8"))
                return res["choices"][0]["message"]["content"].strip()

        # Bible query
        ans1 = ask("quien es el padre de david")
        print("Q1 (Bible): quien es el padre de david ->", ans1)
        assert "Jesse" in ans1

        # Geo/Wikipedia query
        ans2 = ask("capital de francia")
        print("Q2 (Geo): capital de francia ->", ans2)
        assert "Paris" in ans2

        ans3 = ask("es paris la capital de francia")
        print("Q3 (Geo bool): es paris la capital de francia ->", ans3)
        assert "Si" in ans3

        # Honest unknown
        ans4 = ask("quien es el rey de babilonia")
        print("Q4 (Unknown): quien es el rey de babilonia ->", ans4)
        assert "No tengo constancia" in ans4 or "don't know" in ans4.lower()

        print("ALL MULTICORPUS SERVER CHECKS PASSED!")
    finally:
        server_proc.terminate()
        server_proc.wait(timeout=3)

if __name__ == "__main__":
    run_server_test()

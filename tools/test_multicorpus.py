import subprocess
import sys

def test_multicorpus():
    cmd = [
        "build-gcc/chat_main.exe",
        "data/bible/bible_relations.tsv",
        "data/samples/geo_knowledge.tsv"
    ]
    queries = [
        "quien es el padre de david",
        "capital de francia",
        "capital de espana",
        "es paris la capital de francia",
        "quien es el rey de babilonia",
        "salir"
    ]
    input_text = "\n".join(queries) + "\n"
    proc = subprocess.run(
        cmd,
        input=input_text,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace"
    )
    print("STDOUT:\n", proc.stdout)
    if proc.stderr:
        print("STDERR:\n", proc.stderr)
    assert proc.returncode == 0, f"Exit code {proc.returncode}"
    print("TEST COMPLETED")

if __name__ == "__main__":
    test_multicorpus()

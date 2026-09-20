import subprocess
import sys

def test_multicorpus():
    cmd = [
        "build-gcc/chat_main.exe",
        "data/texts/bible.txt",
        "data/texts/jung.txt"
    ]
    queries = [
        "quien es el padre de david",
        "quien es Jung?",
        "dime las areas que conoces",
        "inicia una conversacion",
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

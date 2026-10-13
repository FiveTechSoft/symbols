"""Exercise the actual diagnostic consumer, not a helper-only adapter.
Label gold is explicit syntax evidence; 1909 and 7 remain ambiguous.
"""
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile

CASES = [{'id': '0', 'text': 'Intro:\n1. Alpha content.\n2. Beta content.\n', 'query': 'Alpha', 'content': 'Alpha content.', 'label': '1.'}, {'id': '1', 'text': 'Intro:\n1. Alpha content.\n2. Beta content.\n', 'query': 'Beta', 'content': 'Beta content.', 'label': '2.'}, {'id': '2', 'text': '12. Alpha content. More detail.\n', 'query': 'Alpha', 'content': 'Alpha content.', 'label': '12.'}, {'id': '3', 'text': '12. Alpha content. More detail.\n', 'query': 'More', 'content': 'More detail.', 'label': None}, {'id': '4', 'text': '1. Primer postulado\nContenido completo.\n', 'query': 'Contenido', 'content': 'Primer postulado\nContenido completo.', 'label': '1.'}, {'id': '5', 'text': 'Intro:\r\n1. Alpha content.\r\n', 'query': 'Alpha', 'content': 'Alpha content.', 'label': '1.'}, {'id': '6', 'text': 'Count 7. Next sentence.\n', 'query': 'Next', 'content': 'Next sentence.', 'label': None}, {'id': '7', 'text': 'Value 1.7 seconds.\n', 'query': 'seconds', 'content': 'Value 1.7 seconds.', 'label': None}, {'id': '8', 'text': 'Year 2026. Next year.\n', 'query': 'Next', 'content': 'Next year.', 'label': None}, {'id': '9', 'text': 'Measure 7. Next measure.\n', 'query': 'Next', 'content': 'Next measure.', 'label': None}, {'id': '10', 'text': 'Version 1.2.3 remains.\n', 'query': 'Version', 'content': 'Version 1.2.3 remains.', 'label': None}, {'id': '11', 'text': 'Date 2026-10-12 remains.\n', 'query': 'Date', 'content': 'Date 2026-10-12 remains.', 'label': None}, {'id': '12', 'text': '1.\nContenido.\n', 'query': 'Contenido', 'content': 'Contenido.', 'label': None}, {'id': '13', 'text': '  1. Alpha content.\n', 'query': 'Alpha', 'content': 'Alpha content.', 'label': None}, {'id': '14', 'text': '1. Alpha.\n    2. Beta.\n', 'query': 'Beta', 'content': 'Beta.', 'label': None}, {'id': '15', 'text': '1) Alpha content.\n', 'query': 'Alpha', 'content': '1) Alpha content.', 'label': None}, {'id': '16', 'text': 'a) Alpha content.\n', 'query': 'Alpha', 'content': 'a) Alpha content.', 'label': None}, {'id': '17', 'text': '1.\tAlpha content.\n', 'query': 'Alpha', 'content': 'Alpha content.', 'label': None}, {'id': '18', 'text': '1.  Alpha content.\n', 'query': 'Alpha', 'content': 'Alpha content.', 'label': None}, {'id': '19', 'text': '1. Extra. Beta content.\n', 'query': 'Beta', 'content': 'Beta content.', 'label': None}, {'id': '20', 'text': '7. Next sentence.\n', 'query': 'Next', 'content': 'Next sentence.', 'label': '7.'}, {'id': '21', 'text': 'Reference:\n1909. This is the work.\n', 'query': 'This', 'content': 'This is the work.', 'label': '1909.'}, {'id': '22', 'text': 'Formula f(x)=x^2 remains.\n', 'query': 'Formula', 'content': 'Formula f(x)=x^2 remains.', 'label': None}, {'id': '23', 'text': 'Code foo(1.7); Count 7. Next sentence.\n', 'query': 'Next', 'content': 'Next sentence.', 'label': None}]

def run(exe, path, mode, query):
    result = subprocess.run([exe, str(path), mode, query], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, (mode, query, result.returncode, result.stderr)
    return result.stdout, [json.loads(line) for line in result.stdout.splitlines()]

def main():
    exe = sys.argv[1]
    with tempfile.TemporaryDirectory(prefix="relprobe-context-") as directory:
        path = pathlib.Path(directory) / "source.txt"
        for case in CASES:
            data = case["text"].encode("utf-8")
            path.write_bytes(data)
            plain_stdout, plain = run(exe, path, "--cite", case["query"])
            context_stdout, context = run(exe, path, "--cite-context", case["query"])
            assert plain and len(plain) == len(context), case["id"]
            assert context_stdout == run(exe, path, "--cite-context", case["query"])[0]
            assert plain_stdout == run(exe, path, "--cite", case["query"])[0]
            for before, after in zip(plain, context):
                assert before == {key: after[key] for key in before}, (case["id"], "legacy fields changed")
                assert after["source_sha256"] == hashlib.sha256(data).hexdigest()
                assert after["source_bytes"] == len(data)
                assert bytes.fromhex(after["quote_hex"]) == data[after["first"]:after["end"]]
                assert bytes.fromhex(after["combined_hex"]) == data[after["combined_first"]:after["combined_end"]]
                assert after["combined_end"] == after["end"]
                if not after["has_label"]:
                    assert after["label_first"] == after["label_end"] == 0
                    assert after["label_hex"] == ""
                    assert after["combined_first"] == after["first"]
                    assert after["combined_hex"] == after["quote_hex"]
                else:
                    assert bytes.fromhex(after["label_hex"]) == data[after["label_first"]:after["label_end"]]
                    assert after["label_end"] + 1 == after["first"]
                    assert data[after["label_end"]:after["first"]] == b" "
            hit = context[0]
            content_first = data.index(case["content"].encode())
            assert hit["first"] == content_first and hit["end"] == content_first + len(case["content"].encode()), case["id"]
            assert bytes.fromhex(hit["quote_hex"]) == case["content"].encode(), (case["id"], "quote must remain content-only")
            label = case["label"]
            assert hit["has_label"] == int(label is not None), (case["id"], "label gold")
            assert bytes.fromhex(hit["label_hex"]) == (label.encode() if label else b""), (case["id"], "label bytes")
            if label:
                first = content_first - len(label.encode()) - 1
                assert hit["label_first"] == hit["combined_first"] == first
                assert hit["label_end"] == first + len(label.encode())
                assert bytes.fromhex(hit["combined_hex"]) == (label + " " + case["content"]).encode()
        path.write_bytes(b"Alpha content.\n")
        assert run(exe, path, "--cite-context", "zzabsence20261013")[1] == []
    print("PASS actual relprobe context: 24 syntax fixtures, two processes, unchanged content receipts; year/7 ambiguity retained")

if __name__ == "__main__":
    main()

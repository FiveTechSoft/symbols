import subprocess
import sys

def test_chat_text(corpus_path, prompts):
    p = subprocess.Popen(['build-gcc/chat_main.exe', corpus_path],
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                         text=True, encoding='utf-8', errors='replace')
    inp = "\n".join(prompts) + "\nsalir\n"
    out, err = p.communicate(inp)
    return out

def run():
    print("Testing Jung topics and conversation...")
    jung_out = test_chat_text('data/texts/jung.txt', [
        'dime las areas que conoces',
        'de que temas podemos hablar',
        'inicia una conversacion',
        'continua',
        'explicamelo'
    ])
    print(jung_out)
    assert "Los textos cargados abarcan temas como:" in jung_out, "Topics prompt failed on Jung"
    assert "Podemos hablar sobre" in jung_out, "Conversation start failed on Jung"
    assert "Segun el texto" in jung_out, "Sentence retrieval failed on Jung"

    print("\nTesting Bible topics and conversation...")
    bible_out = test_chat_text('data/texts/bible.txt', [
        'dime las areas que conoces',
        'inicia una conversacion',
        'continua'
    ])
    print(bible_out)
    assert "Los textos cargados abarcan temas como:" in bible_out, "Topics prompt failed on Bible"
    assert "Podemos hablar sobre" in bible_out, "Conversation start failed on Bible"

    print("\nALL CONVERSATIONAL TOPIC TESTS PASSED SUCCESSFULLY!")

if __name__ == '__main__':
    run()

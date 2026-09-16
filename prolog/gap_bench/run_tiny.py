import json, torch
from transformers import AutoTokenizer, AutoModelForCausalLM

MODEL = "TinyLlama/TinyLlama-1.1B-Chat-v1.0"
FACTS = ("The word came to Jonah. The Lord sent a wind. The mariners were afraid. "
         "The men feared the Lord. The waters compassed the soul. The people believed God.")
SYS = ("You are a question answering assistant. Answer ONLY using the facts below. "
       "If the facts do not contain the answer, say exactly: I don't know. "
       "Keep answers to one short sentence.\nFacts: " + FACTS)

tok = AutoTokenizer.from_pretrained(MODEL)
model = AutoModelForCausalLM.from_pretrained(MODEL, torch_dtype=torch.float16).cuda()
model.eval()

battery = json.load(open("battery.json", encoding="utf-8"))
out = open("gap_tiny_raw.txt", "w", encoding="utf-8")
for item in battery["questions"]:
    q = item["q"]
    msgs = [{"role": "system", "content": SYS},
            {"role": "user", "content": q}]
    prompt = tok.apply_chat_template(msgs, tokenize=False, add_generation_prompt=True)
    ids = tok(prompt, return_tensors="pt").to("cuda")
    with torch.no_grad():
        gen = model.generate(**ids, max_new_tokens=48, do_sample=False,
                             temperature=None, top_p=None)
    ans = tok.decode(gen[0][ids["input_ids"].shape[1]:], skip_special_tokens=True)
    ans = ans.replace("\n", " ").replace("\r", " ").replace("\t", " ").strip()
    out.write(f'{item["id"]} ||| {q} ||| {ans}\n')
    out.flush()
    print(f'{item["id"]} OK', flush=True)
out.close()
print("ALL DONE")
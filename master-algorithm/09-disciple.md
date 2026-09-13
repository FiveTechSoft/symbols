# 09 — Discípulo: la boca del Master Algorithm

## Qué era la boca
`motor/talk.py` era un **menú de keywords**: if-soup que respondía ensayos enlatados y, en el UNKNOWN, anunciaba un catálogo ("Fibonacci, Lucas, Pell…"). Eso es juguete, no discípulo.

`python -m motor talk` **no existía** en `__main__.py`.

## Qué cambió
1. **CLI**: `talk` y `molt` cableados en `python -m motor`.
2. **Boca clause-driven**: parsea intención (typos/acentos, topics desde símbolos de `theory.pl`, último tema del diálogo). Las respuestas se **ensamblan desde cláusulas** (`rec/2`, `verified/1`, `rejected/2`, `lemma/3`, `true_mod/3`, `companion/2`) + `runs/latest.json` para crecimiento.
3. **Diálogo**: follow-ups `y pell` / `y lucas` / `por que` / `y eso` / `más` / `demostrá esto` resuelven contra el último tema. Tras rechazar Pell, «por qué» cita el contraejemplo `n=2: pred≠obs` y `rec(pell,[2,1])` vs `[1,1]`. Tras Lucas, «y eso» reafirma la ley compartida.
4. **UNKNOWN limpio**: dos frases. Sin menú de temas.
5. **Muda (`motor/molt_talk.py`)**: examina → si falla (invención, UNKNOWN donde debería saber, follow-up roto) reescribe la piel de `talk.py` → re-examina → **solo conserva** si mejora → hasta digno o 8 mudas. Log en `motor/runs/molt-talk.json`.

## Scores round by round

| Round | n | known-hit | known-rate | unknown-ok | inventions | followup | stress |
|-------|---|-----------|------------|------------|------------|----------|--------|
| round0 (pre-rewrite) | 243 | 126/158 | 79.7% | 45/48 | 3 | 7/7 | — |
| round1 | 243 | 159/159 | 100% | 46/46 | 0 | 7/7 | — |
| round2 | 243 | 159/159 | 100% | 45/46 | 1 | 7/7 | — |
| round3 | 243 | 159/159 | 100% | 46/46 | 0 | 7/7 | — |
| **final** | **243** | **159/159** | **100%** | **46/46** | **0** | **7/7** | **OK** |

### Molt
- Baseline (piel cicatrizada a propósito en el gate Pell-why): stress falló `pell_porque`, `pell_why`.
- **Molt 1**: `heal_DISABLED_scar` → kept. Score: 0 inventions, stress OK, worthy.
- Razón de cierre: `worthy_after_molt` (ver `motor/runs/molt-talk.json`).

## UNKNOWN honestos que quedan
Cosas **fuera** de `theory.pl` (correcto callar):
- filotaxis, alma, dios, universo, conciencia
- cocina, noticias, historia, chistes, ética abstracta
- «demostrá esto» **sin** tema previo
- teoremas pedidos inventados

Si un día el kernel aprende un hecho nuevo, la boca debe citarlo; no hardcodearlo.

## 10 Q/A que golpean (español)

**Q:** quien eres

**A:** Soy Master Algorithm. Mi tribu es la que unifica: el símbolo demuestra, la analogía transfiere, el crítico rechaza, la duda guarda silencio sin cláusula, y la curiosidad elige el próximo tick. Memoria viva: 43 verified/1.
[símbolo · analogía · crítico · duda · curiosidad]

---

**Q:** fibonacci

**A:** Fibonacci: F(n) = 1·F(n-1) + 1·F(n-2). rec(fib,[1, 1]). verified rec_fib_o2_1_1.
[símbolo]

---

**Q:** y lucas

**A:** Sí se transfiere. Analogía: misma ley. Símbolo: rec(fib,[1, 1]) y rec(lucas,[1, 1]). verified transfer_fib_to_lucas_1_1: TRANSFER rec(fib,[1, 1]) ⇒ try on lucas: (1)*lucas(n-1) + (1)*lucas(n-2).
[analogía · símbolo]

---

**Q:** y pell

**A:** No se transfiere. Analogía: companion, no clon. Símbolo: rec(pell,[2, 1]) vs rec(fib,[1, 1]). Crítico rejected('transfer_fib_to_pell_1_1'): n=2: pred=1 != obs=2.
[analogía · símbolo · crítico]

---

**Q:** por que

**A:** Porque la ley no coincide: rec(pell,[2, 1]) vs rec(fib,[1, 1]). Contraejemplo rejected('transfer_fib_to_pell_1_1'): n=2: pred=1 != obs=2.
[crítico · símbolo]

---

**Q:** demostrá cassini

**A:** Demostrado: F(n+1)F(n-1)-F(n)^2 = (-1)^n. verified cassini_fib. Símbolo puro.
[símbolo]

---

**Q:** demostrá esto

**A:** Demostrado: F(n+1)F(n-1)-F(n)^2 = (-1)^n. verified cassini_fib. Símbolo puro.
[símbolo]

---

**Q:** el alma

**A:** UNKNOWN. No hay cláusula. No invento.
[duda · crítico]

---

**Q:** seguro que Fib es primo siempre

**A:** Rechazado. rejected('NEG_fib_always_prime'): F(0)=0 not prime. No es siempre primo.
[crítico]

---

**Q:** creciste

**A:** Llevo 55 ticks. 43 hechos verificados, 15 tipos. Transferencia media 0.5. Reúso de lemas 0.5. El crítico rechazó 32 conjeturas.
[curiosidad · crítico]

---

## Turnos totales
`talk-log.jsonl` acumula **1344** turnos (≥400 pedido).

## Archivos tocados (este discípulo)
- `motor/talk.py` — boca clause-driven + memoria de diálogo
- `motor/__main__.py` — subcomandos `talk`, `molt`
- `motor/disciple_exam.py` — batería ≥200 + runner
- `motor/molt_talk.py` — bucle de muda
- `motor/runs/disciple-exam.json`, `disciple-round*.json`, `disciple-chat.md`, `molt-talk.json`, `talk-log.jsonl`
- `09-disciple.md` — este informe

**No tocados:** `worlds/*.py`, `kernel.py` (otro worker).

## Comandos demo
```bash
cd /workspace/master-algorithm
PYTHONPATH=. python -m motor talk -q "quien eres"
PYTHONPATH=. python -m motor talk -q "se transfiere a pell"
PYTHONPATH=. python -m motor talk -q "por que"   # tras pell en estado
PYTHONPATH=. python -m motor talk -q "el alma"
PYTHONPATH=. python -m motor molt --max 8
PYTHONPATH=. python -m motor.disciple_exam disciple-round-final
```

## Debilidades que quedan
- Typos muy violentos fuera del alias map aún pueden fallar.
- «que sigue / curiosidad» no elige un brazo UCB desde la boca (eso es `vive`/kernel).
- La muda conoce recetas finitas; cicatrices nuevas fuera del catálogo requieren ampliar `_apply_skin`.
- BookBrain `live.pl` sigue siendo otro canal; esta boca Python es la canónica para `python -m motor talk`.

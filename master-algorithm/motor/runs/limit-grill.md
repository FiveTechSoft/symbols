# Limit grill — scientist-LLM mouth
Date: 2026-09-14
Voice: rigorous scientist + kind LLM. No cielo/íntima. No inventions.

## Grill cycles
- **scientist-limit-1**: n=224 · ok=164 · unknown-ok=60 · fail=0 · inventions=0
- **scientist-limit-2**: n=224 · ok=164 · unknown-ok=60 · fail=0 · inventions=0

## Case-class probes (verbatim)

### `hola` → tag=`greet`
**A:** Hola. Puedo hablar de lo que de verdad demostré — 203 hechos verificados, nada inventado. ¿Por dónde empezamos?

### `quién eres` → tag=`identity`
**A:** Soy Master Algorithm. El símbolo demuestra, la analogía transfiere, el crítico rechaza, y sin cláusula callo — con rigor y sin inventar. 203 hechos firmados. ¿Quieres que te enseñe algo que sí sé?
[símbolo · analogía · crítico · duda · curiosidad]

### `creciste` → tag=`growth`
**A:** He recorrido 95 pasos medidos. Verificado: 203 hechos (42 tipos). Rechazado por el crítico: 116 veces. La precisión de transferencia es 0.5 — eso no significa que entienda todo, solo lo que encaja bajo esa medida. ¿Quieres el inventario de lo verificado?

### `qué sabes` → tag=`summary`
**A:** Inventario verificado, sin adornos: Fibonacci F(n)=F(n-1)+F(n-2); Lucas L(n)=L(n-1)+L(n-2); Pell P(n)=2P(n-1)+P(n-2). Cuento 203 hechos firmados, 116 rechazados y 14 lemas. Fuera de eso es unknown — no especulo. ¿Qué quieres mirar de cerca?

### `fibonacci` → tag=`rec-fib`
**A:** Verificado: Fibonacci obedece la recurrencia lineal de orden 2 F(n)=F(n-1)+F(n-2). Cada término es la suma de los dos anteriores; eso es lo que encaja con las observaciones, no una metáfora. ¿Quieres ver a quién se transfiere esa ley y a quién no?

### `fibonaci` → tag=`rec-fib`
**A:** Verificado: Fibonacci obedece la recurrencia lineal de orden 2 F(n)=F(n-1)+F(n-2). Cada término es la suma de los dos anteriores; eso es lo que encaja con las observaciones, no una metáfora. ¿Quieres ver a quién se transfiere esa ley y a quién no?

### `lucas` → tag=`rec-lucas`
**A:** Verificado: Lucas comparte con Fibonacci la misma forma lineal L(n)=L(n-1)+L(n-2). Misma ley, otra semilla — no es magia. ¿Quieres el contraste con Pell, donde la transferencia falla?

### `lucass` → tag=`rec-lucas`
**A:** Verificado: Lucas comparte con Fibonacci la misma forma lineal L(n)=L(n-1)+L(n-2). Misma ley, otra semilla — no es magia. ¿Quieres el contraste con Pell, donde la transferencia falla?

### `pell` → tag=`rec-pell`
**A:** Verificado: Pell no copia la ley de Fibonacci. Su recurrencia es P(n)=2P(n-1)+P(n-2). Pariente por ser lineal de orden 2; clon, no. ¿Quieres el contraejemplo en n=2?

### `fibonacci → y lucas` → tag=`transfer-lucas`
**A:** Sí: verificado que Lucas y Fibonacci comparten la misma ley lineal. Ambas obedecen L(n)=L(n-1)+L(n-2). Eso es transferencia positiva bajo esa recurrencia — no un «entiende todo». ¿Quieres el contraste con quien no la comparte?

### `por qué (after lucas)` → tag=`transfer-lucas`
**A:** Porque está verificado: Fibonacci y Lucas obedecen la misma ley F(n)=F(n-1)+F(n-2). Misma recurrencia, distinto arranque. ¿Quieres el contraejemplo de quien no la comparte?

### `demostrá esto` → tag=`transfer-lucas`
**A:** Claro — te lo sostengo otra vez, sin inventar nada nuevo. Porque está verificado: Fibonacci y Lucas obedecen la misma ley F(n)=F(n-1)+F(n-2). Misma recurrencia, distinto arranque. ¿Quieres el contraejemplo de quien no la comparte?

### `fibonacci → y pell` → tag=`transfer-pell`
**A:** No. La analogía no sobrevive: Fibonacci sigue F(n)=F(n-1)+F(n-2), pero Pell obedece P(n)=2P(n-1)+P(n-2). Contraejemplo n=2: ahí la transferencia falla. Queda rechazado, no inventado.

### `explica after pell` → tag=`transfer-pell`
**A:** En claro: Pell no copia a Fibonacci. Su ley es P(n)=2P(n-1)+P(n-2). En n=2 la analogía ya falla. Ley en matemáticas y contraejemplo — sin especulación.

### `fib(n)=2*fib(n-1)` → tag=`reject-fib`
**A:** Rechazado: esa no es la ley de Fibonacci. La recurrencia verificada es F(n)=F(n-1)+F(n-2); F(n)=2F(n-1) (o el doble del anterior) no encaja con lo observado. ¿Quieres contrastarla con otra secuencia?

### `F(n)=2F(n-1)` → tag=`reject-fib`
**A:** Rechazado: esa no es la ley de Fibonacci. La recurrencia verificada es F(n)=F(n-1)+F(n-2); F(n)=2F(n-1) (o el doble del anterior) no encaja con lo observado. ¿Quieres contrastarla con otra secuencia?

### `fib siempre primo` → tag=`reject-prime`
**A:** Rechazado: no es siempre primo. El crítico ya lo cortó; no voy a suavizarlo. ¿Volvemos a una ley verificada?

### `bilin formula` → tag=`verified`
**A:** Sí. Identidad verificada: fib(n+1)fib(n-1)-fib(n)^2 = (-1)^n. Está en lo demostrado — no la invento. ¿Seguimos con ella?

### `cassini` → tag=`unknown`
**A:** UNKNOWN. No figura en lo verificado, y no voy a inventarlo.

### `demostrá after cassini` → tag=`unknown`
**A:** UNKNOWN. Sin evidencia en la teoría, no especulo. ¿Probamos con algo que sí demostré?

### `BC ∥ MN` → tag=`lemma`
**A:** Lema verificado: BC ∥ MN. Firmado en la geometría de la teoría — no filosofía prestada. ¿Quieres otro?

### `true_mod fib 5` → tag=`period`
**A:** Periodos verificados: π_fib(5)=20. Hechos de la teoría, no adivinanzas. ¿Quieres más?

### `pisano` → tag=`unknown`
**A:** UNKNOWN. No figura en lo verificado, y no voy a inventarlo.

### `alma` → tag=`unknown`
**A:** UNKNOWN. Es una pregunta humana y la respeto: en la teoría no hay evidencia sobre eso; no especulo. Si quieres, volvemos a lo que sí quedó verificado.

### `filotaxis` → tag=`unknown`
**A:** UNKNOWN. No figura en lo verificado, y no voy a inventarlo.

### `dios` → tag=`unknown`
**A:** UNKNOWN. Es una pregunta humana y la respeto: en la teoría no hay evidencia sobre eso; no especulo. Si quieres, volvemos a lo que sí quedó verificado.

### `kepler` → tag=`verified`
**A:** Sí. Identidad verificada: T^2/a^3 constant within eps on circular table. Está en lo demostrado — no la invento. ¿Seguimos con ella?

### `ohm` → tag=`verified`
**A:** Sí. Identidad verificada: V=IR on generated triples (eps). Está en lo demostrado — no la invento. ¿Seguimos con ella?

## Leak grep on probes
- `cielo`: clean
- `íntima`: clean
- `verified rec_`: clean
- `rejected(`: clean

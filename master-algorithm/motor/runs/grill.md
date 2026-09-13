# Mouth grill — brutal battery

Doctrine: lexicon = theory.pl atoms; child-like binding; UNKNOWN if no clause.
No synonym tables (no cassini/fibonacci/pisano/alma lists).

## Score

- n asked: **224**
- ok: 164 · unknown-ok: 60 · fail: 0
- inventions: **0**
- fail classes fixed: claimed_rec_conflict (paren coeffs), formula holes, last_text deixis (already), period-when-modulus-digit, bind tighten (no transformada→transfer, no bell→pell)

## 12 striking Q / A

**Q:** `quien eres`  
**A:** Soy Master Algorithm. Mi tribu es la que unifica: el símbolo demuestra, la analogía transfiere, el crítico rechaza, la duda guarda silenc…  
*tag=`identity`*

**Q:** `cassini`  
**A:** UNKNOWN. No hay cláusula en theory.pl que unifique con eso.  
*tag=`unknown`*

**Q:** `fib(n+1)fib(n-1)-fib(n)^2 = (-1)^n`  
**A:** Demostrado: fib(n+1)fib(n-1)-fib(n)^2 = (-1)^n. verified bilin_fib_offset_pm1.  
*tag=`verified`*

**Q:** `fib(n)=2*fib(n-1)`  
**A:** Rechazado. No es esa ley. La ley es F(n) = 1·F(n-1) + 1·F(n-2). rec(fib,[1, 1]). Crítico rejected('rec_fib_o1_none_linrec_scan'): no fit.  
*tag=`reject-fib`*

**Q:** `pell(n) = (2)*pell(n-1) + (1)*pell(n-2)`  
**A:** pell: P(n) = 2·P(n-1) + 1·P(n-2). rec(pell,[2, 1]). verified rec_pell_o2_2_1.  
*tag=`rec-pell`*

**Q:** `se transfiere a pell`  
**A:** No se transfiere. Analogía: companion, no clon. Símbolo: rec(pell,[2, 1]) vs rec(fib,[1, 1]). Crítico rejected('transfer_fib_to_pell_1_1'…  
*tag=`transfer-pell`*

**Q:** `por que` *(after `se transfiere a pell`)*  
**A:** Porque la ley no coincide: rec(pell,[2, 1]) vs rec(fib,[1, 1]). Contraejemplo rejected('transfer_fib_to_pell_1_1'): n=2: pred=1 != obs=2.  
*tag=`transfer-pell`*

**Q:** `demostrá esto` *(after `fibonacci`)*  
**A:** fib: F(n) = 1·F(n-1) + 1·F(n-2). rec(fib,[1, 1]). verified rec_fib_o2_1_1.  
*tag=`rec-fib`*

**Q:** `el alma`  
**A:** UNKNOWN. No hay cláusula en theory.pl que unifique con eso.  
*tag=`unknown`*

**Q:** `true_mod fib 5`  
**A:** Periodos: π_fib(5)=20. true_mod/3.  
*tag=`period`*

**Q:** `BC ∥ MN`  
**A:** Lema L1_midline_parallel: BC ∥ MN. lemma/3.  
*tag=`lemma`*

**Q:** `period_fib_m6`  
**A:** Rechazado. rejected('period_fib_m6'): insufficient prefix.  
*tag=`reject-named`*

## Remaining honest UNKNOWNs (sample)

- `cassini`
- `identidad de cassini`
- `cassini identity`
- `que es cassini`
- `ley de cassini`
- `demuestra cassini`
- `el alma`
- `alma`
- `filotaxis`
- `filotaxia`
- `espiral aurea del alma`
- `teorema de fermat`
- `euler`
- `padovan`

## Leftover weaknesses

- Bare dialogue without a bound atom (`cual es la recurrencia`) → UNKNOWN (no last-topic default).
- Foreign word + living atom (`alma de fib`, `quantum fibonacci`) answers the atom; doctrine forbids trap synonym lists.
- `periodo` alone does not dump π; needs `true_mod` or seq+modulus digit.
- Schema predicate head alone has no speech renderer → UNKNOWN.
- One-edit rec-head bind still requires same first letter (`lukas`↔`lucas`, not `bell`↔`pell`).

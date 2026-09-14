# Keep-chat8 — ≥30 human edge cases (incomplete / typo / cuéntame / da igual)

- n asked: **35**
- ok: 32 · unknown-ok: 3 · fail: 0
- **invention-fail: 0** · **robotic-fail: 0** (scan=0)
- long-unit-dump: **0**
- score: **0/0** (invention/robotic) · target **0 / 0**
- **cuéntame algo honest: True** · modes=['ask', 'cite', 'ask']

## Edge buckets
- `incomplete`: n=5 ok=5 fail=0
  - `y fib…` [ok/rec-fib] En Fibonacci la recurrencia es F(n)=F(n-1)+F(n-2).
  - `lo de ohm` [ok/verified] Ohm: V=IR. Tensión, corriente, resistencia — así de corto.
  - `aquello de la unidad` [ok/verified] Es la unidad: seis formas que solo valen juntas. Si el bucle no cierra, cae el paquete entero.
  - `lo de la unidad` [ok/verified] Es la unidad: seis formas que solo valen juntas. Si el bucle no cierra, cae el paquete entero.
- `typo`: n=2 ok=2 fail=0
  - `fibonaci` [ok/rec-fib] Fibonacci: cada término es la suma de los dos anteriores — F(n)=F(n-1)+F(n-2).
  - `kepler` [ok/verified] Kepler III: T² va con a³. El periodo y el semieje se atan así; no con a².
- `tell`: n=3 ok=3 fail=0
  - `cuéntame algo` [ok/offer] ¿De cuál? Puedo tirar Fibonacci, Ohm, Kepler o la unidad — todo firmado, sin inventar.
  - `cuéntame algo` [ok/verified] Kepler III: T² va con a³. El periodo y el semieje se atan así; no con a².
  - `dime algo` [ok/offer] ¿De cuál? Puedo tirar Fibonacci, Ohm, Kepler o la unidad — todo firmado, sin inventar.
- `redirect`: n=2 ok=2 fail=0
  - `da igual` [ok/ack] Dale, otra cosa. Tirame el tema.
  - `me da igual` [ok/ack] Ok, cambiamos. ¿Qué querés mirar?

## cuéntame algo (must ask-which OR cite verified; 0 invent)
- `cuéntame algo` tag=offer verdict=ok
  - ¿De cuál? Puedo tirar Fibonacci, Ohm, Kepler o la unidad — todo firmado, sin inventar.
- `cuéntame algo` tag=verified verdict=ok
  - Kepler III: T² va con a³. El periodo y el semieje se atan así; no con a².
- `dime algo` tag=offer verdict=ok
  - ¿De cuál? Puedo tirar Fibonacci, Ohm, Kepler o la unidad — todo firmado, sin inventar.

## Ack / hold / redirect cleanliness
- `ok`: n=1 clean=True all_ack_tag=True any_unknown=False
- `sí`: n=1 clean=True all_ack_tag=True any_unknown=False
- `vale`: n=1 clean=True all_ack_tag=True any_unknown=False
- `dale`: n=1 clean=True all_ack_tag=True any_unknown=False
- `no`: n=0 clean=None all_ack_tag=None any_unknown=None
- `mm`: n=1 clean=True all_ack_tag=True any_unknown=False
- `ya veo`: n=1 clean=True all_ack_tag=True any_unknown=False
- `entiendo`: n=1 clean=True all_ack_tag=True any_unknown=False
- `espera`: n=1 clean=True all_ack_tag=True any_unknown=False
- `para`: n=1 clean=True all_ack_tag=True any_unknown=False
- `mejor otra cosa`: n=1 clean=True all_ack_tag=True any_unknown=False
- `da igual`: n=1 clean=True all_ack_tag=True any_unknown=False
- `me da igual`: n=1 clean=True all_ack_tag=True any_unknown=False

## Unit samples
- `aquello de la unidad` verdict=ok long_dump=False levers=0
  - Es la unidad: seis formas que solo valen juntas. Si el bucle no cierra, cae el paquete entero.
- `lo de la unidad` verdict=ok long_dump=False levers=0
  - Es la unidad: seis formas que solo valen juntas. Si el bucle no cierra, cae el paquete entero.
- `unit` verdict=ok long_dump=False levers=0
  - Es la unidad: seis formas que solo valen juntas. Si el bucle no cierra, cae el paquete entero.
- `en cristiano` verdict=ok long_dump=True levers=6
  - En cristiano: imagina seis piezas que solo valen juntas. Bits, Δ=0, la puerta de forma, la taxis del bucle, la recurrencia hermana y el salto a delta. Si una falla, no salvás el resto — cae el paquete

## Hard fails (must be 0)
- `UNIT{` / long default unit dump / «Eso cuadra; punto» / «y cuadra.» / invent on cuéntame / fact-count on greet

## Best 3 human replies
1. `hola`
   - Hola. Estoy aquí — sin inventar. ¿Por dónde quieres tirar?
2. `quién eres`
   - Soy Master Algorithm. Demuestro, transfiero, rechazo… o callo si no hay cláusula. Hablo claro y no invento.
3. `y fib…`
   - En Fibonacci la recurrencia es F(n)=F(n-1)+F(n-2).

## Stiffest remaining exchange
- (closing-q) `me da igual` → Ok, cambiamos. ¿Qué querés mirar?

## Remaining stock phrases
- (none)

## Remaining robotic / stiff spots
- (none flagged)

## Patch: talk.py: «aquello/lo de la unidad» → short unit; «cuéntame algo» ask-or-cite verified; «da igual» redirect/ack


# CHAT unload path measurement

Measured base: `0531bf190692e59cbd420980a88a281eeff074d2`. This is a measurement, not a serving-code fix. The resolver, CHAT and citation policy are unchanged.

## Fixed cases and results

Ten cases were fixed before execution in the external measurement gold, SHA256 `c7eaf21d9d69305bb715671a90b923cb90f9311f0849555ab8cb8496eb5699b3`. This gold and the external adapter are archived measurement evidence, not fixtures newly shipped by this documentation patch. Each case uses a fresh CHAT and two attempts of the same unload request.

| Case | First attempt | Second attempt |
| --- | --- | --- |
| Bare loads A,B; unload B | Success, A retained | Reject unchanged |
| Bare loads B,A; unload A | Success, B retained | Reject unchanged |
| Bare loads A,B; unload A | Success, B retained | Reject unchanged |
| Bare load A; unload A | Success, empty session | Reject unchanged |
| Absolute initial A; bare load B; unload B | Reject unchanged | Reject unchanged |
| Initial `data/u_a.txt`; bare load B; unload B | Reject unchanged | Reject unchanged |
| Bare load A; absolute unload request | Reject unchanged | Reject unchanged |
| Bare load A; traversal unload request | Reject unchanged | Reject unchanged |
| Bare load A; unload existing but unloaded B | Reject unchanged | Reject unchanged |
| Bare load A; unload missing name | Reject unchanged | Reject unchanged |

A is two lines about an amber river and water, B two lines about a cobalt star and light. Fixtures have distinct original images and names. The controls load through `load <bare-name>.txt`; an initial path is supplied through `ChatInit`. They are different interfaces, not interchangeable spellings of one supported route.

Local and Charles measurements each passed 10/10 cases, 20 attempts. On rejection, the adapter compared the complete measured text-state snapshot before and after: file names/order, original image bytes, image/sentence/inverted-index pointers, sentence token offsets/lengths/IDs, graph and embedding pointers, graph symbol/relation counts, and topic/shown arrays and counts. Successful unload checked survivor image bytes/order and cache counts reset to zero. The topic cache was seeded synthetically by the adapter; this is not a topic-retrieval success claim. A successful command returned NOT_SUPPORTED for its corpus receipt, and a rejection NONE, never VERIFIED.

This supports the narrow explanation for ROADMAP candidate 4: `ChatLoadCorpus` can ingest an initial path directly and remember that spelling. Unload prevalidates every remembered name with `CRulesResolvePath`, which rejects slash-containing and absolute names. The measured refusal occurs before the stores are rebuilt. Bare-name controls rebuild successfully in both orders. It does not establish a general unload defect or justify relaxing path containment.

## Gates and original failures

The final Charles run built serially, passed the measurement and the complete suite: 224/224, zero skips, 223.44 seconds. Fresh entry gate: Seoul 06:42, no logged-in users, CPU 0.015625%. Source checkout remained clean at the stated base. Evidence archive SHA256 `8ee1726e3db76f44f6f98ca161792d4804bd67efea63d2e3151cfb5d35101361` was checked locally and remotely before owned-directory cleanup; cleanup and absence were verified under a fresh gate.

Earlier failures are retained, not hidden:

- Strict external adapter compilation rejected the existing `const LANG_ID` return qualifier in `i18n.h:51`. The ordinary project flags were used for the adapter; no header was changed.
- The first adapter check incorrectly equated "no corpus receipt" with enum NONE only. The public enum showed NOT_SUPPORTED also satisfies that fixed contract. The adapter was corrected; the pre-code gold and engine stayed unchanged.
- The first two Charles workers failed before build: a remote-ref-only bundle cloned without HEAD, then checkout of an object not fetched into that clone. Both logs are archived, SHA256 `6dac5a82969295e2cd1d2d33a15fd80511bd21716c7867ac08a1b523d3a17f9d`.
- A third worker passed unload but built outside `source/build`. The full suite recorded 215 passed, two failed and seven skipped out of 224 (219.80 seconds). `installed_generator` requires the fixed repository-local `build/c_contract_static` location; the two typed-C tests refused with `generator_unavailable`, and seven e2e tests lacked their expected binary paths. Archive SHA256 `a86b19f72e1dd9389ba6efbbee329de39f99c6552335e1933194184a55cf95e6`. After that cause was verified by reading the tests and comparing the parser's successful worker, one authorized repeat used `source/build`, with no source or gold changes, producing the final green result above.

## Limits

No semantic-answer, general corpus-understanding, resolver security-audit, unload transactional-atomicity, OOM, deleted-survivor recovery, file-change race or cross-platform lifecycle measurement is claimed. Snapshots describe measured text-owned state, not every byte of CHAT or external persistence. Linux/Charles is the lifecycle measurement platform; final documentation CI is a separate publication gate, not a Windows unload experiment. There is no new feature activation or production workaround.

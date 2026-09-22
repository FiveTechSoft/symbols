# External benchmark manifest: real C errors for bounded deterministic repair

`test_agent_runner_external` measures the bounded repair loop
(`compiler-did-you-mean`, `missing-header`) against errors taken from, or
faithfully minimized from, real public C commits and small public projects.
No evaluation case was used to develop the operators, and no operator or test
encodes an evaluation identifier.

## Method and legality

- Cases marked **commit** below are minimized from the cited public commit.
  The fixture is a fresh, minimal reproduction of the commit's diagnostic
  shape (same identifier class, same compiler diagnostic), not a copy of the
  project's source. Where the original identifier would not trigger the same
  GCC suggestion behavior, the adaptation is noted.
- Cases marked **modeled** reproduce a real project's public API name and
  module split (`.c` consumer, `.h` declarer) to instantiate the
  missing-include diagnostic class; the fixture code is original.
- Cases marked **constructed** are boundary probes built for this benchmark
  (no external source).
- All upstream projects carry permissive or public-domain licenses
  (MIT, BSD-2-Clause, BSD-3-Clause, zlib, Unlicense/public domain), verified
  against each repository's license file. Fixture code here is original work
  of this repository; upstream identifiers are used only as provenance
  references.
- Fully offline: every fixture is vendored; CTest needs no network access.

Compiler reference for diagnostic shapes: GCC 11.4 (`cc -fsyntax-only`),
flags per case in `tests/test_agent_runner_external.c`.

## Evaluation cases (`evaluation/`)

| Case | Family | Expected | Source | License | Notes |
|---|---|---|---|---|---|
| dym_stb_hmget | did-you-mean | resolved | [nothings/stb@498bd3e](https://github.com/nothings/stb/commit/498bd3e01719448abca314153f38e36fe8b5043e) | MIT/Unlicense | Real typo: call `stbds_hmget_key` for `stbds_hmget_key_ts`. Minimized to single line. |
| dym_stb_unpremultiply | did-you-mean | resolved | [nothings/stb@4d160de](https://github.com/nothings/stb/commit/4d160de463003e8e35660eeb1ed4891419dddbdb) | MIT/Unlicense | Real name mismatch: `stbi__unpremultiply_on_load_thread` vs `stbi_set_...`. |
| dym_stb_packset | did-you-mean | resolved | [nothings/stb@8cf07e8](https://github.com/nothings/stb/commit/8cf07e85c8b64841981e35cd005e2fcb0762eec7) | MIT/Unlicense | Stale rename: caller kept `stbtt_PackSetSkipMissingGlyphs` after rename to `...Codepoints`. |
| dym_tct_timespec | did-you-mean | resolved | [tinycthread/tinycthread@701bed1](https://github.com/tinycthread/tinycthread/commit/701bed1bc1fe4985a3c19f96345f027cca9d80c0) | zlib | Real struct-tag typo `_ttherad_timespec`; minimized as a typedef-name typo (same diagnostic class). |
| dym_acutest_assign | did-you-mean | resolved | [mity/acutest@2521116](https://github.com/mity/acutest/commit/25211166b9d675acae2521b38230e4604a6b436a) | MIT | Real variable typo `assignement` -> `assignment`. |
| dym_member_tvsec | did-you-mean | resolved | constructed | - | Member-access typo (`tv_seconds` vs `tv_sec`); instantiates the `has no member named ... did you mean` class. |
| hdr_cjson_fn | missing-header | resolved | modeled on [DaveGamble/cJSON](https://github.com/DaveGamble/cJSON) (`cJSON_Utils.h` API) | MIT | Real public name `cJSONUtils_Compare`; consumer missing the include. |
| hdr_inih_fn | missing-header | resolved | modeled on [benhoyt/inih](https://github.com/benhoyt/inih) (`ini.h`) | BSD-3-Clause | Real public names `ini_parse`, `ini_handler`. |
| hdr_sds_fn | missing-header | resolved | modeled on [antirez/sds](https://github.com/antirez/sds) (`sds.h`) | BSD-2-Clause | Real public names `sds`, `sdsnewlen`; header under `include/`. |
| hdr_jansson_fn | missing-header | resolved | modeled on [akheron/jansson](https://github.com/akheron/jansson) (`jansson.h`) | MIT | Real public name `json_object_seed`. |
| hdr_jansson_type | missing-header | resolved | modeled on [akheron/jansson](https://github.com/akheron/jansson) | MIT | Real public type `json_t`; `unknown type name` shape; header under `src/`. |
| hdr_tct_type | missing-header | resolved | modeled on [tinycthread/tinycthread](https://github.com/tinycthread/tinycthread) (`tinycthread.h`) | zlib | Real public type `mtx_t`; `unknown type name` shape. |
| neg_kilo_uint32 | negative | abstain | [antirez/kilo@262d556](https://github.com/antirez/kilo/commit/262d5567728abe5c61a0d2b6cccdc48c5d641bee) | BSD-2-Clause | Real fix added `#include <stdint.h>` for `UINT32_MAX`. System header + macro: outside operator scope, must abstain. |
| neg_stb_hashseed | negative | abstain | [nothings/stb@40adb99](https://github.com/nothings/stb/commit/40adb995abeea13612ad73bda031c90e3c0cf821) | MIT/Unlicense | Real typo `stbds_BB` vs `stbds_hash_seed`; GCC 11.4 offers no suggestion, must abstain. |
| neg_stb_arraddn | negative | abstain | [nothings/stb@2d82cd1](https://github.com/nothings/stb/commit/2d82cd1a2c0b8c8db1eac55d84f736a3aed9bb0e) | MIT/Unlicense | Real rename `arraddnoff` -> `arraddnindex`; GCC 11.4 offers no suggestion, must abstain. |
| neg_dup_callsite | negative | abstain | constructed (derived from stb@498bd3e shape) | - | Suggestion exists but the identifier occurs twice in the proposed body; unique-token precondition must reject. |
| neg_impl_only | negative | abstain | constructed | - | Symbol defined only in a sibling `.c`; zero header candidates. |
| neg_other_file | negative | abstain | constructed | - | Root diagnostic is in `other.c`, not the task target; path-exact gate must reject. |
| amb_two_headers | ambiguity | abstain | constructed | - | Two local headers each declare the symbol once; ambiguous, must abstain. |
| amb_two_types | ambiguity | abstain | constructed | - | Two local headers each typedef the type once; ambiguous, must abstain. |
| ooc_c89_loop | out-of-coverage | abstain | [antirez/linenoise@8c1c63c](https://github.com/antirez/linenoise/commit/8c1c63c5fdbdc7540d21522d735501f81b3ae80c) | BSD-2-Clause | Real C89 failure: `for`-loop initial declaration. Diagnostic class unclaimed. |
| ooc_ptr_int_cmp | out-of-coverage | abstain | [sheredom/utf8.h@cbbb787](https://github.com/sheredom/utf8.h/commit/cbbb787968a5c760d47b384b4930993b88dc89a5) | Unlicense | Real pointer/integer comparison break, adapted to a plain comparison (same diagnostic class). |
| ooc_ptr_sign | out-of-coverage | abstain | [sheredom/utf8.h@146be69](https://github.com/sheredom/utf8.h/commit/146be69f88575d753317d8ef13b16f80e0656fc7) | Unlicense | Real signedness error under `-Werror=pointer-sign`. |
| ooc_arity | out-of-coverage | abstain | constructed | - | `too few arguments to function`; arity class unclaimed. |
| ooc_syntax | out-of-coverage | abstain | constructed | - | Missing `;`; syntax class unclaimed. |

## Development cases (`development/`)

Synthetic harness sanity checks only: one resolvable case per operator, one
no-suggestion abstention, one syntax abstention. They share no identifiers
with the evaluation set.

## Reproduction

```sh
cmake -S . -B build && cmake --build build -j
cd build && ctest -R test_agent_runner_external --output-on-failure
```

The harness prints one line per case plus a `METRICS` line per suite:
resolution rate, false positives, correct abstentions, patch-only baseline,
attempts, replans and CPU time. The patch-only baseline (initial hunk alone,
no repair loop) must resolve zero cases; it is the control that isolates the
operators' marginal value.

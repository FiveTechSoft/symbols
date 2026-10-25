# Test-only supplied-record question support contract

Local research increment, base remote9064ca74f4c8252cfacb6f572249157e417a0f1a. No serving hook, parser adapter, KB tuples, source corpus changes, new CTest target, workflow or activation. QSCheckRecords is static in a test-only header included solely by test_chat_citation.c. No fuzzy normalization.

## Meaning

QS_MATCH means ANNOTATION_RELATIVE_MATCH. semantic_evidence is always QS_UNVERIFIED_EXTRACTION, including E11: fabricated semantic annotations can match while their raw spans are valid. No result certifies prose entailment, automatic extraction correctness, real shell behavior or world truth. Completeness is supplied annotation assertion, not inferred from span coverage. origin arrays prove byte ownership only. QS_OPPOSITE is also annotation-relative, not a general contradiction oracle.

Request states COMPLETE/PARTIAL/FORM_UNSUPPORTED/LIMIT; completeness_origin1 means supplied complete,2 uncertain. Set enumeration1 complete,2 incomplete. Assertions are supplied manual or extraction-uncertain; grammar rule_id must be0 because no adapter is implemented. Field kinds1..12 are subject,relation,value,condition,implementation,platform,version,polarity,quantifier,source selector,mode,context. Field requirement mandatory or not-applicable; presence explicit/unknown/not-requested. Not-requested requires not-applicable. Explicit values must lie wholly within at least one declared origin, all origins within their owning query/assertion. Duplicate field kinds, including identical duplicates, are invalid. No origin or array index is trusted before bounds validation.

Structural invalidity (bad enum/reserved/span/hash/duplicate) returns INVALID. Valid requests beyond feature/resource limits return UNSUPPORTED_FORM with reason LIMIT. Counts exceeding arrays return limit before any element access. Output is cleared on every call; source/query SHA-256 is recomputed inside the function with existing TLSha256. Callers may not pass forged equality flags. Same-source assertions are hash-checked before matching. Zero-element complete set returns NO_MATCH; incomplete returns UNKNOWN. A candidate's decisive explicit mismatch excludes it even if another field or extraction state is unknown. Without a decisive mismatch, relevant uncertainty blocks MATCH and NO_MATCH. Invalid records invalidate the whole call, including unrelated candidates. Complete-set enumeration is needed for NO_MATCH only. Missing mandatory subject/relation is UNKNOWN. Multiple matching assertions conservatively return UNKNOWN/conflict, even if identical: deduplication is outside this increment. Same-scope affirmative/opposite conflict is order-independent UNKNOWN. A negative assertion yields OPPOSITE only when a proposition value is explicit; a negative lookup without value remains UNKNOWN.

Comparison records link assertion index and field kind to the two immutable records, retaining both origin spans there. There is no display answer, delivery state or core GOAL change. No persistence/session state is read or written. Caller owns buffers and must not alias result with inputs; this ordinary C non-aliasing precondition is not dynamically detectable. Memory allocation validity beyond supplied lengths is a caller precondition.

## Frozen directed cases and observed validation

The development plan preceded execution. Tests use fictional values, not real shell facts: query `mode returns blue set Luma Vela 2`, source extends that with `clear Neri Arin 3 stores export`. Record annotation fields are explicit. No live engine query or exposed80 replay was added. Existing citation tests still run because they are the target hosting these pure tests; their original body is unchanged.

Coverage: exact match; every mandatory field changed independently; complete/incomplete empty sets; partial and multi-goal request; missing implementation/platform/version/condition; unknown extraction; decisive platform mismatch with unknown condition or extraction; explicit opposite versus condition mismatch; no-information; same-scope conflict and reversed order; unrelated implementation candidate and reversed order; query/source identity corruption; overflow-shaped span; valid origin but value outside origin; duplicate request/assertion kind including identical duplicates; invalid enums/reserved/rule_id; nonzero unused origin; all count/resource limits; explicit completeness uncertainty; E11 unverified extraction; output reset/null inputs; no input-record mutation; relevant unknown alongside matching candidate; worst-case capacities. Full original lexical/parsing contrastives E01/E02/E03 are deferred because no adapter exists: this increment does not pretend to parse word order, late tokens, UTF-8 normalization or source sentence structure.

GCC Linux Release: existing test_chat_citation target 1000 assertions pass, including132 new pure checks. New pure checks also pass ASan+UBSan, leak detection on, no diagnostics. Standalone pure-check executable is scratch evidence, not a new registered test or repository pipeline. Original citation-test body byte-compared against base after subtracting new block/call. No full252 suite run is needed or claimed for this test-only increment; specifically no Git-mutating test was run. Test clone came from remote archive, git-init with independent metadata, zero remotes or credential config, not a worktree. No closed CI was queried.

## Bounds and measured costs

Caller-owned exact sizes checked by compile-time typedef assertions and printed by GCC: span8,identity40,field56,request728,assertion736,set5896,result1568 bytes. Total principal metadata8192 bytes, caller buffers separate. Test-only before/after snapshots and maximum byte buffers have static storage, not nested stack arrays. Query<=4096 bytes,source<=65536,one goal,eight assertions,twelve fields,four origins. No dynamic allocation or IO in verifier. Hash cost O(query+source), hashes each buffer once; compares each assertion identity32bytes. At most80 field-value comparisons and327680 byte comparisons (8*10*4096). The maximum fixture attains327680 comparison bytes,80 comparisons,69632 hashed bytes. Counter cannot overflow at these bounds. Each assertion field search scans<=12; duplicate validation<=66 pair checks per record; origin checks<=48 per record. No constant-time or latency claim.

GCC -O2 -fstack-usage (not sanitizer layout): QSCheckRecords192bytes,QSFieldsValid56bytes,TLSha256240bytes,its block384bytes. Maximum hash call-chain estimate192+240+384=816bytes, plus ABI/caller stack; max field validation chain248bytes. Report compiler artifacts with patch review. Stack is compiler/config-dependent, not a universal ABI promise; no MSVC/Windows size/runtime measurement yet.

## Next adapter is NOT implemented or authorized

The previously proposed `How are <subject> assigned?` / `<subject> are assigned <predicate>.` connection is only a design lead. Free subject/predicate captures can swallow qualifiers or compound requests. Before implementing it, define a genuinely bounded grammar and ambiguous-case rejection oracle, including suffixes within captures, conjunctions, version/condition phrasing, punctuation, source heading scope and full-input consumption. A real corpus question such as Bash variable assignment is an opportunity, not a measured answer capability. This verifier alone cannot improve live answers or validate extraction truth.

## Review corrections v0.3

All mandatory request fields must be explicit before considering ANY candidate set, including empty or decisively unrelated sets. Unknown request bounds always return UNKNOWN after valid structural/identity checks. Assertion-side unknowns remain excludable by an explicit decisive mismatch; these are different cases.

Single polarity/mode encoding: source polarity is assertion_kind only. Request with mandatory explicit value is an affirmative proposition; request without mandatory value is a lookup. Negative requested propositions are not represented in this increment. Explicit fieldkind8 polarity or fieldkind11 mode on either side returns UNSUPPORTED_FORM, for affirmative and negative assertions alike, preventing equality filtering from silently overriding assertion_kind. Those slots may be not-requested/not-applicable only. E11 now swaps implementation/platform role kinds on both manual records: Luma bytes are falsely annotated as platform, while all spans and SHA remain valid. The record matcher still matches, and semantic_evidence stays UNVERIFIED_EXTRACTION; it does not detect the semantic lie. Max matching fields are now10 per record due to reserved unsupported polarity/mode encodings.

## Assignment grammar demo v1 (supersedes size/rule restrictions above)

Test-only demo, no serving integration or new engine queries. Query origin_rule is explicit uint32:0 supplied/manual,1 assignment grammar. Request sizeof732, principal metadata8196bytes. Source extraction_state3=GRAMMAR_RECORD and rule_id1. Manual0 remains unchanged. Unknown rule and mixed manual/grammar arms are INVALID. Caller may still deliberately relabel BOTH records manual; that is the existing supplied-annotation/E11 boundary, never extraction proof. Every result remains UNVERIFIED_EXTRACTION and quoted-span-relative.

Closed byte-exact ASCII grammar:

```
ID := [A-Za-z][A-Za-z0-9_]{0,30}
ATOM := [A-Za-z_][A-Za-z0-9_]{0,30}
Q := 'How are ' ID ' variables assigned?'
S := ID ' variables are assigned with ' ATOM '=' ATOM '.'
   | ID ' variables are assigned with ' ATOM '=' ATOM
     ' and must not contain spaces around the equals sign.'
```

No free subject/predicate capture, translation, fuzzy normalization, shell-name list, heading inheritance or context. ID is an opaque subject label, not a typed implementation. Subject origin includes `ID variables`; relation origin is literal `assigned`; source value is the complete `with ...` including the spacing caveat, excluding terminal period. Unsupported suffix/conjunction/negation/condition/version/multiline/case/whitespace variants are rejected as grammar coverage misses, not semantic safety successes. CMD's existing assignment+reading sentence is intentionally rejected; no prefix clipping.

Pure QSParseAssignmentQuery and QSExtractAssignmentSentence clear outputs, construct hashes and exact records only after whole-input/whole-span grammar success. QSCheckRecords re-runs both adapters on original bytes, compares all members including every unused field/origin slot, and rejects forged state/completeness/count/origins/kinds/values/identity/rule. Comparison is member-wise, not padding-dependent memcmp. Manual count overflow remains UNSUPPORTED_FORM/limit; recognized grammar-record count forgery is INVALID before array reads. Recognized rule must never be accepted based on a numeric caller flag alone.

Gold was specified before execution: real Bash assignment sentence with complete caveat; synthetic Luma simple assignment; subject mismatch; query version suffix, version inside subject, compound subject/relation, negation, condition, second request, lowercase/doubled space/newline/nonASCII/longID; source set+reading,condition,exception,negation,second sentence,heading,version subject,incomplete/extra equals,other conjunction,trailing space/double period; forged request and source records, unused arrays, unknown/mixed rules, bounds/count/identity mutation. Existing132 pure checks remain intact. Added141checks yield273pure and1141total in existing citation target, all PASS GCCRelease. Pure ASanUBSan leakON273PASS,0diagnostics. Full original citation code body remains unchanged except invoking the added pure test blocks. No full252local suite or closedCI recheck claimed.

Real-corpus end-to-end fixture reads unchanged data/shells/shell_corpus.txt through fixed repo-relative path under existing sllm_test WORKING_DIRECTORY CMAKE_SOURCE_DIR, checks source18094bytes SHAdaefd50882510d9c4f12fa847c915d246371f6cc59e622201909b12d0ec7f294 and exact line18 span[1457,1552). Query `How are Bash variables assigned?` produces grammarkey1 records, source origins are absolute within full file. QSCheckRecords yields annotation/grammar-relative MATCH with UNVERIFIED_EXTRACTION, not a default Symbols/OpenCode answer. Mutating source invalidates records. A selected standalone sentence inside `# obsolete examples` can parse, but its heading context is not certified; the test explicitly exposes this limitation rather than claiming global support.

Measured GCC-O2stack: QSCheckRecords192,QSGrammarRequestValid768,QSParseAssignmentQuery64,QSGrammarAssertionValid768,QSExtractAssignmentSentence112,TLSha256240+block384bytes. Deepest estimated chain192+768+112+240+384=1696bytes plus ABI/caller; original citation main remains23712bytes. No heap or local source copies. Rule revalidation hashes query once plus once in common verifier, and source once per grammar assertion plus once in common verifier. At8assertions/maxbuffers: hash bytes<=2*4096+9*65536=598016 (hash_bytes field still counts common identity check only, NOT total revalidation work). Existing manual maximum byte comparisons327680 remains; grammar match compares only short subject/relation fields, full record validation checks fixed arrays. No timing/constant-time promise or measured Windows/MSVCclaim. Callers must provide nonaliased valid memory; malformed pointers outside provided lengths cannot be detected in C.

Practical value: demonstrates byte-origin-preserving parsing plus evidence checking for one closed documentary relation, and prevents partial-match qualifier loss for that grammar. Essential OpenCode gap: no runtime route calls it; it cannot interpret ordinary shell questions or arbitrary prose, source context/completeness remains uncertified, and no live answer improves. Further relations or serving work need separate review, not incremental exceptions to improve this bank.

### Fixture-path and bound correction

No __FILE__ dependency remains. Existing CMakeLists.txt sllm_test definition sets the target working directory to CMAKE_SOURCE_DIR. Open exactly data/shells/shell_corpus.txt from that root, no upward search or alternative directory guesses, and require the pinned SHA/byte length. Thus slash/backslash/relative compiler __FILE__ spellings are irrelevant; none is parsed. Direct invocation outside source root is not the supported fixture context; CTest supplies it on each platform. Future corpus content changes must fail this explicit pin until the fixture/oracle is reviewed and updated, never refresh hashes automatically.

QSName stops on the32nd identifier byte instead of scanning arbitrarily long identifiers. Checks include31accepted/32rejected/emptyID,NULL0query,NULL0source,full4096query/full65536source and overcaps. No byte beyond caller count is read. Other grammar comparisons use bounded constant literals. These tests do not constitute a Windows execution claim; CI after approved publication would own that validation.

## Portable checkout fixture correction (after MSVC failure)

Publication29f7980's MSVC job121045691926 failed only the three real-corpus pin/span checks; fopen and other new/legacy checks passed. Local LF-to-CRLF conversion reproduced those exact three failures. Remote log did not print actual source SHA/length, so CRLF is a strongly reproduced explanation, NOT a measured remote fingerprint. No engine regression or general Windows semantic failure is established.

The test now accepts exactly two separately pinned original byte images, never normalizes the image being certified:

| Representation | Byte length | SHA-256 | Line18 span |
| --- | --- | --- | --- |
| canonical repository LF | 18094 | daefd50882510d9c4f12fa847c915d246371f6cc59e622201909b12d0ec7f294 | [1457,1552) |
| explicit LF->CRLF checkout image | 18292 | cd527960cf247a8fb4b6a63781b9e71e2a30cbc5136dba988e38428cdcc8f581 | [1474,1569) |

as_checkout_span recomputes SHA over actual bytes, selects a representation by hash AND length, derives line18 offsets from the SAME image, then requires those offsets and exact sentence bytes to match that representation's pin. It does not equate canonical SHA with checkout SHA. Every query/source record and verifier origin uses actual bytes. Unknown representation, mixed line endings, wrong length, bitflip or other-representation span fails. No .gitattributes, workflow, CMake or corpus change. Future canonical content changes need explicit oracle review and deliberate replacement of BOTH pinned representations, not automatic hash refresh.

An in-memory image transform creates the other representation only as explicit test data; the input corpus is not rewritten by the test. Both image forms run full adapter+verifier end-to-end. Mutants cover representation-span swap, record SHA mutation, source bitflip, mixed-EOL and length mismatch. Local full target run on LF and on a temporarily CRLF checkout each1157PASS, original canonical corpus restored exact SHA afterwards. Pure sanitizer289PASS with leakON, zero diagnostics; prior273pure preserved except replacing the five failing LF-only fixture checks with representation-aware checks. No claim of fixed MSVC CI until new approved publication is actually checked. No retry of the original failed run.

## Local real action/scope serving increment (25 October 2026)

Unlike the test-only sections above, this increment changes serving CHAT for one
closed documentary form. It is directed development, not independent evaluation,
not a shell executor and not general prose entailment. Outside the form, legacy
behavior is unchanged and uncertified by this increment.

Whole ASCII query: optional `I know COMMAND on SCOPE. ` followed by
`How do I VERB a OBJECT in SCOPE?`. Whole source: `Upper(VERB)ing a OBJECT uses
COMMAND in SCOPE and COMMAND in SCOPE` optionally followed by
`, with no direct SCOPE builtin`, then a period. VERB and object words are lowercase
ASCII, 1..31 bytes. OBJECT has 1..6 words. SCOPE is a case-sensitive identifier,
1..31 bytes. COMMAND is 1..8 tokens of letters/digits/underscore/hyphen, each
1..63 bytes. Structural delimiters and listed condition/negation words are
reserved; quotes, compound scopes, third alternatives, trailing context and
unknown morphology are unsupported. No synonyms, heading inheritance, shell
names or command lists supply semantics. Source candidate must parse whole,
not a clipped clause. Repeated target scope or target-negative suffix conflicts.
A command cannot consume the delimiter: `in`, `on`, `and`, `uses`, `not`, `if`,
`unless`, `when` are reserved command tokens, making this parse deterministic.

One private ParseIntent adapter runs only after existing ParseIntentToks success,
only with a loaded documentary session and NONE/QA/TEXTQ result, and only on a
whole-input parse. It does not replace failed/action/load intents. Self, fastpath,
plan, Resolve and semantic guards keep their order. Private PARSED contains a
borrowed pointer valid only for that synchronous call; public ChatParse copies
value slots, and no CHAT, plan or session stores the pointer. Query is reparsed
from the original immutable bytes before use. Query cap is 1024 bytes, overflow
fails closed, not truncated. Existing planner token arrays remain unchanged.

Within TEXTQ the verb, object and target supply the same retrieval mechanism,
with existing top16 and no score/threshold changes. Context scope is not target.
Only one positive candidate without conflicts emits its whole literal source;
zero matches/conflicts produce unknown. A per-call private emission records
that this closed match passed; legacy truncated lexical verification does not
reclassify it. Existing citation hashes the full file image and binds the quote.
It certifies the documentary target-command binding under this grammar, not
world truth, builtin absence on every installation, execution, source authenticity
or general answer correctness. Exhaustiveness/conflict checking is limited to
retrieved candidates, not a complete scan of all corpus statements.

Canonical shells source: 18094 bytes, SHA256
`daefd50882510d9c4f12fa847c915d246371f6cc59e622201909b12d0ec7f294`.
Comparison content [15345,15454) binds Bash command [15374,15381) and PowerShell
command [15394,15411); PowerShell target [15415,15425). Full original question,
including optional preface and target beyond the old token budget, is retained.
Actual full-query retrieval places this comparison second of top16 for the
PowerShell question. Display stays the whole comparison, not a synthesized command.

Directed checks cover simple/prefaced/fresh/context/long questions, inverse target,
CMD negative suffix, unknown scope, changed action/object, duplicated scopes,
fictional and permuted alternatives, invalid source/query boundaries, oversized
input and caller-buffer lifetime. Actual OpenCode 1.18.32 with the local Symbols
binary displays useful comparison answers for PowerShell/Bash; CMD and unknown
scope abstain. These are narrow directed acceptance cases, not exposed80 accuracy.
Initial baseline simple CMD incorrectly returned the comparison with unsupported
citation; its failure is preserved in evidence. The final simple/prefaced CMD
cases abstain, which is not a claim of universal builtin absence.

Historical v1 Linux native suite reconciled by bounded serial blocks: 252 tests registered,
245 PASS, seven explicitly SKIPPED integration gates. No interrupted invocation
counts as completion. First parallel git-gate calls failed on shared index.lock;
separate independent archive/git-init validation ran both serially PASS. Those
tests also committed their containing disposable repository, so patch comparison
uses explicit base 9840c67d473d84b0b5b66c03866d4696543bdf2f, not current HEAD.
Metadata/remotes/common-dir identities were inspected; no shared operational git
metadata is used. First typed-C preview/verify checks failed because the generator
was built outside their fixed expected build path. Copying the existing local
compiled c_contract_static into that expected scratch build path made both PASS;
no source/workflow/tool change or install was made. Existing citation target also
passes ASan+UBSan with leak detection, zero sanitizer diagnostics. These local
results do not substitute for publication review or later Windows CI.

### Review v2 corrections (local, not published)

The bounded query length loop tests its numeric limit before reading the next
byte. Both 1024-byte malformed and 1025-byte oversized envelopes reject rather
than admit truncated forms. Grammar limits have 31/32 identifier, 6/7 object-word
and 8/9 command-token boundary tests. The isolated endpoint mutant reparses a
fresh request before changing only the source endpoint.

The per-call action emission does not itself authorize a lexical-guard bypass.
ChatQuoteVerified rechecks the current file/sentence citation, literal quote
length and bytes, whole original query and source comparison, and target binding
before accepting it. Small caller outputs retain explicit output_truncated rather
than certify a complete displayed quote. Current-image mutations do not reuse
old parsed binding. ParseIntentToks clears the entire private PARSED at entry
before its first return or recursive parse; planner copies use this entry, while
public ChatParse returns copied value slots only.

The public matcher compares supplied query length and target boundaries, then
uses re-parsed query/source fields. It does not certify integrity of every field
in the caller-supplied assertion structure. Those fields grant no proof.

Seven prior native skips were all missing server binaries in the scripts' fixed
build/build-gcc lookup paths, not unsupported platforms. An existing server built
from the correct source/config was fingerprinted and placed in each independent
clone's expected build path. Candidate seven gates now execute PASS without SKIP.
Original failed/skip logs remain evidence, not promoted results. Full comparable
baseline validation is separately recorded, not inferred from the candidate.

Initial runtime evidence stored the last transport as the answer, which could be
a later title-generation call. That v1 summary is preserved as discrepant. The v2
receipt classifies requests by their actual system/user body and response ID.
Title-generation output is never counted as the documentary answer. Current
recaptures bind /proc/exe hash to the final compiled binary and retain before/after
source/corpus hashes and raw final/title streams separately. They do not prove
historical process identity retroactively.

### Final local v2 validation

Candidate and explicit archive810d0510 baseline each complete 252/252 native
PASS, zero SKIP, reconciled from completed serial bounded blocks. Earlier
interrupted calls are not completion evidence. Candidate citation target has
1251 directed assertions PASS (including both conflicting-candidate orders),
ASan+UBSan with leak detection enabled and zero sanitizer diagnostics. Initial
baseline benchmark refused an archive lacking git subject identity; independent
git-init supplied that required local identity, then the benchmark PASS. No
operational git common-dir or remote is shared. Built server and typed-generator
source/config/path/SHA fingerprints accompany the evidence. This is local
regression evidence, not an independent accuracy evaluation or a Windows claim.

The final executable differs from v1 because of the reviewed guard corrections.
Final long-query and CMD recaptures bind the new /proc/exe SHA to the compiled
binary, current source/corpus before/after hashes, explicit ports/config, request
body and response ID. Title and answer streams are classified separately. Long
id1 is title and id2 the comparison; CMD id1 is title and id2 abstention. No
historical screenshot is authenticated retroactively by this current receipt.

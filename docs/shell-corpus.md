# Shell knowledge corpus: provenance and evaluation

`data/shells/shell_corpus.txt` covers Bash (Linux), CMD, PowerShell, and macOS/Darwin shells,
plus cross-shell equivalences and portability traps. Requested by Antonio 2026-10-24
("implementa un corpus para un conocimiento profundo de los shells. Pruebalo a fondo").

## Provenance and verification status

Original content authored for this project on 2026-10-24; no manual text was copied.
Authoring alone does not prove accuracy, so claims carry these anchors:

- Bash claims: verified by real execution on GNU bash 5.x / Linux, 20/20 checks
  (brace expansion, `${#var}`, `${var:-default}`, `$?`, pipefail, sed -i GNU semantics,
  grep -c, awk fields, find/xargs -0 with spaces, chmod +x, kill -9 exit 137, [[ == pattern ]],
  globbing, tr -d CR, sort -u, cut on /etc/passwd, du -sh, arithmetic expansion).
- "The default shell on macOS is zsh": Apple Support, Terminal User Guide
  (support.apple.com/guide/terminal/change-the-default-shell-trml113). The attribution
  "since macOS Catalina 10.15 (2019)" follows Apple's 2019 announcement; the bash-3.2/GPLv3
  rationale is long-documented but not re-verified against an Apple primary source here.
- "Windows PowerShell 5.1 vs PowerShell 7 default output encodings differ": Microsoft Learn,
  about_Character_Encoding ("In Windows PowerShell, any Unicode encoding, except UTF7,
  always creates a BOM. PowerShell (v6 and higher) defaults to utf8NoBOM").
- CMD, PowerShell, and Darwin-specific claims beyond those anchors are NOT verified by
  execution in this environment (no Windows or macOS runner available here). They are
  documentation-level statements and must be labeled as such until run on their OS.

## Evaluation (frozen bank, pre-registered)

Question bank: 80 questions, frozen as sha256 57f3af78e6000792f12f193177b3bffb2e5e5f7f6090bfa586fe446c7f7cdace
at 2026-10-24T07:12:23Z, before the first run, and never used to tune corpus or router.
The bank was authored by the same author as the corpus, so it is pre-registered, not independent.

Results against the engine (symbols-server, build of master 5b80fbf, corpus loaded via /v1/model/load):

- paraphrase retrieval: 14/25
- cross-shell translation: 7/20
- negative (out-of-corpus, must abstain): 14/20
- concept traps: 9/15
- total: 44/80 (55%) with whitespace-normalized scoring (37/80 raw)

Failure classes observed (recorded, not fixed by this change):

1. Wrong-domain fallback: out-of-corpus questions answered with a C code skeleton or the
   indexed-repo summary instead of abstaining (6/6 negative failures). Honesty defect for
   the any-corpus goal.
2. Hot-sentence misretrieval: one sed/awk sentence returned for unrelated questions.
3. False abstention: in-corpus questions answered "I don't know." (~12 cases).
4. Trap near-miss: contrast sentence retrieved instead of the exact one.

Ingestion integrity: 149 sentences / 1033 symbols; idempotent reingest via /v1/model/load
(same counts); absent terms (tmux, jq, fish, schtasks, iptables, zstd) return 0 corpus hits.
Corpus file sha256: daefd50882510d9c4f12fa847c915d246371f6cc59e622201909b12d0ec7f294.

OpenCode acceptance (real TUI, OpenCode 1.18.32, Symbols as model): `/load data/shells/shell_corpus.txt`
typed into OpenCode reached the server and loaded the corpus (server log: 149 sentences, 1033 symbols) —
the literal /load step through the TUI is proven. Literal exchanges: macOS services question answered
with the launchd content; the "macOS ships Bash 5" trap answered with the version-3.2/GPLv3 content;
"tmux sessions" answered "I don't know." (status=UNKNOWN). Screenshots archived with the run notes.

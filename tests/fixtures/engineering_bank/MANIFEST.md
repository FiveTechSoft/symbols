# Engineering task bank

- Tasks: 59
- Categories: compiler_repair, refactor, test_authoring, build_ci, docs, shell, multi_file, debug, git
- Contract: see COORDINATION.md
- Self-test: `python scripts/test_engineering_bank.py`
- `after/` is golden end-state for self-test only; never show it to the agent under test.
- `setup.py` builds state a file tree cannot hold (git history); `golden.py`
  is the self-test golden operation for those tasks (run after setup).

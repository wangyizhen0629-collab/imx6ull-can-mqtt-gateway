# M10 Windows merge review

- Run ID: 20260902T135700p0800-m10-windows-merge-review
- Local time: 2026-09-02T14:01:36.8937061+08:00
- Windows analyzer regression (8 tests): **PASS**
- Imported/final artifact manifests (9, 158 entries): **PASS**
- Staged sensitive-information scan (172 files): **FAIL**
- Staged git diff --cached --check: **PASS**
- Overall: **FAIL**

## Scope boundary

- Board, CAN, Broker and STM32 operations: **NOT RUN** (outside the authorized merge-review scope).
- Long-duration and real-hardware tests: **NOT RUN**.
- M10 gate remains **NOT MET**; this review does not start M11.

## Notes

- The only merge conflict was `.gitattributes`; the Windows preflight rule and all eight feature-branch artifact rules were retained.
- The analyzer was run with `-B` to avoid creating Python bytecode files.
- The sensitive scan read the staged Git blobs, not historical untracked directories.
- A first invocation stopped at PowerShell parse time, and a second stopped in output-function parameter binding before the analyzer began. Both runner failures are retained; no test result was claimed from either invocation.

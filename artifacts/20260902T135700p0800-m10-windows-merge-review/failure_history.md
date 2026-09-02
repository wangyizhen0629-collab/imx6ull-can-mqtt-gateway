# Non-final failure history

- The first invocation of `run_merge_review.ps1` exited 1 during PowerShell parsing at the dynamically generated Markdown report.
- Cause: Markdown backticks inside double-quoted PowerShell strings escaped closing delimiters.
- Impact: the script did not begin the analyzer, manifest, sensitive-scan or diff checks and produced no result files.
- Correction: removed those Markdown backticks from dynamic strings; the corrected script was then rerun from a clean evidence-output state.

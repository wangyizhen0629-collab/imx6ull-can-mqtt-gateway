# Second non-final runner failure

- The second invocation entered `run_merge_review.ps1` and wrote `01_context.txt`, then exited 1 while binding the first `Invoke-NativeCapture` call.
- Cause: the array concatenation passed to `Write-Utf8 -Lines` lacked enclosing parentheses, so PowerShell treated `+` as an unexpected positional argument.
- Impact: the analyzer and all requested checks still had not begun.
- Correction: parenthesized the complete array expression and retained the already written context file without replacing it.

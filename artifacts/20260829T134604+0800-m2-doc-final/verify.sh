#!/bin/sh

set -eu

AUDIT_DIR="artifacts/20260829T134604+0800-m2-doc-final"
JSON_LIST="$AUDIT_DIR/json_files.txt"

find artifacts -type f -name '*.json' | sort > "$JSON_LIST"
JSON_COUNT=0
while IFS= read -r JSON_FILE; do
    python3 -m json.tool "$JSON_FILE" >/dev/null
    JSON_COUNT=$((JSON_COUNT + 1))
done < "$JSON_LIST"
[ "$JSON_COUNT" -gt 0 ]
printf 'PASS valid_json_files=%s\n' "$JSON_COUNT"

git diff --check
printf 'PASS git_diff_check\n'

grep -q 'M2 已于 2026-08-29 通过' docs/PLANS.md
grep -q '| M2 .*| 2026-08-29 已通过 |' docs/PLANS.md
grep -q '| M3-A .*| 未开始 |' docs/PLANS.md
grep -q '^\*\*PASS。\*\*' docs/milestones/M2.md
grep -q 'M2 退出门禁已满足' docs/OPEN_QUESTIONS.md
grep -q 'M2 已接受并通过' docs/DECISION_LOG.md
grep -q '是，必须保留 loopback 限定且不得写性能' docs/RESUME_TRACEABILITY.md
grep -q 'M1、M2 已完成' README.md
printf 'PASS required_M2_documents_updated_and_M3_not_started\n'

for REQUIRED_DOC in docs/PLANS.md docs/milestones/M2.md docs/OPEN_QUESTIONS.md docs/RESUME_TRACEABILITY.md docs/TEST_PLAN.md; do
    grep -q 'artifacts/20260829T133148+0800-m2-board-loopback/' "$REQUIRED_DOC"
    grep -q 'artifacts/20260829T134148+0800-m2-final-audit/' "$REQUIRED_DOC"
done
printf 'PASS final_board_and_audit_evidence_referenced\n'

grep -q '100% tests passed, 0 tests failed out of 9' artifacts/20260829T131536+0800-m2-host-regression/ctest.log
grep -q '100% tests passed, 0 tests failed out of 9' artifacts/20260829T131705+0800-m2-asan-ubsan-regression/ctest.log
grep -q '"arm_warning_clean_build": "PASS"' artifacts/20260829T131442+0800-m2-arm-cross-final/summary.json
grep -q '"m2_exit_gate": "MET"' artifacts/20260829T133148+0800-m2-board-loopback/summary.json
grep -q '"m2_exit_gate": "MET"' artifacts/20260829T134148+0800-m2-final-audit/summary.json
printf 'PASS host_sanitizer_ARM_board_and_final_audit_gate_results\n'

sh -n tools/can/run_m2_board_loopback.sh
if git status --short | grep -Eq ' stm32/| protocol/'; then
    echo 'unexpected M3-related worktree change' >&2
    exit 1
fi
printf 'PASS board_runner_syntax_and_no_M3_source_changes\n'

printf 'FINAL result=PASS milestone=M2 next_milestone_started=no\n'

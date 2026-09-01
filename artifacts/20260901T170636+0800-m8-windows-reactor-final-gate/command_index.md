# M8 final gate command/evidence index

- Repository/source identity: `gate_preparation.txt`, `binary_handoff.txt`,
  `config_redaction_trace.txt`.
- Target preflight/deployment: `board_readonly_preflight.redacted.txt`,
  `board_deploy.redacted.txt`, `board_setup.redacted.txt`,
  `deploy_snapshot_v2.redacted.txt`.
- Online baseline: `board_online.redacted.txt`, `windows-baseline/`.
- First outage and continuous spool growth: `board_outage1_immediate.redacted.txt`,
  `board_outage1_later.redacted.txt`, `gateway_main1_outage1_alignment2.redacted.txt`.
- Subscriber-before-gateway reconnect alignment:
  `reconnect1_subscriber_before_gateway.txt`; recovery cursor:
  `board_reconnect1.redacted.txt`.
- Second outage/SIGKILL: `board_outage2_immediate.redacted.txt`,
  `board_outage2_later.redacted.txt`, `board_kill_main1.redacted.txt`.
- Same-spool crash recovery: `crash_recovery_subscriber_before_gateway.txt`,
  `board_start_main2.redacted.txt`, `board_crash_recovery.redacted.txt`,
  `board_stop_main2.redacted.txt`.
- State corruption/replay: `board_make_state_copy.redacted.txt`,
  `board_state_recovery.redacted.txt`, `board_stop_state1.redacted.txt`.
- Final cleanup/export: `board_final_audit.redacted.txt`,
  `windows-capture/subscriber-stop-v4.redacted.txt`,
  `windows-crash-recovery/broker-stop-v5.redacted.txt`,
  `fetch_board_export.redacted.txt`.
- Complete safe board/Broker logs: `board-evidence-redacted-v2/`,
  `broker-logs-redacted-v2/`; raw/redacted hashes:
  `public_redaction_v2_trace.txt`; scan: `public_redaction_v2_scan.txt`.
- Broker accounting: `broker_accounting.txt`.
- Subscriber and strict validation: `subscriber.jsonl`,
  `subscriber.redaction-trace.txt`, `validator_command.txt`,
  `validator_command_correction.txt`, `validator_version.txt`,
  `validator_input_sha256.txt`, `validator_selftest.*`, `validator.*`,
  `final_validator_result.txt`.
- Private evidence binding: `raw_private_sha256.txt`; private files themselves
  remain Git-ignored.
- Failures/corrections are preserved in files containing `failure` and in the
  v1 redaction directories, which are intentionally excluded from staging.

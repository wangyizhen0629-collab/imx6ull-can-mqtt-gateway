# Sensitive-scan finding review

The broad staged-blob scan reported four credential-assignment matches. None is a real credential or a newly added secret:

- `gateway/src/main.c:591`: `sink_config.broker_password = config->broker_password;` copies a configuration field; it contains no credential value.
- `gateway/src/mqtt_sink.c:542`: `config->broker_password == NULL` is a null check; it contains no credential value.
- `gateway/tests/test_config.c:51`: `broker_password=fake-test-secret` is an explicit unit-test fixture.
- `gateway/tests/test_config.c:178`: `broker_password=secret` is an explicit unit-test fixture used to validate rejection of an invalid reconnect interval.

`git diff --cached` confirms that none of these four lines is added by the merge. They are retained as reviewed, non-actionable matches. The final scan continues to reject private-key markers, private/link-local IPv4 literals, forbidden credential/private-key filenames, staged binaries, and any credential assignment outside this exact four-line allowlist.

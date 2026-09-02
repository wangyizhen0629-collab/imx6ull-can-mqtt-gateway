# M10 spool v2 ASan+UBSan最终报告

- 配置：Ubuntu x86_64，Debug，`GATEWAY_ENABLE_SANITIZERS=ON`
- Configure/build：PASS
- 环境：`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`，
  `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`
- 全量CTest：21/21 PASS
- sanitizer诊断：未观察到
- LeakSanitizer：`NOT RUN`（明确关闭`detect_leaks`）

这只是主机sanitizer证据，不是板端或性能证据。

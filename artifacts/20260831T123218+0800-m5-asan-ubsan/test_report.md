# M5 ASan 与 UBSan 测试

结论：`PASS`。以 `GATEWAY_ENABLE_SANITIZERS=ON` 构建当前源码，设置 ASan/UBSan
遇错立即停止后，全量 CTest 12/12 PASS，M5 专项直接运行也 PASS。日志中没有
AddressSanitizer 或 UndefinedBehaviorSanitizer 报告。

LeakSanitizer 因当前命令执行环境受 `ptrace` 约束而使用 `detect_leaks=0`，因此明确记为
`NOT RUN`；本 run 不能声称无泄漏。该结果仍是 Ubuntu x86_64 主机结果，不是 ARMv7
或目标板 sanitizer 结果。

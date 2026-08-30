# M4 ASan + UBSan 回归

启用 AddressSanitizer 和 UndefinedBehaviorSanitizer 后完成 warning-clean build，全量
CTest 11/11 PASS，M4 2/2 PASS。因已知 `ptrace` 环境限制，运行时明确设置
`ASAN_OPTIONS=detect_leaks=0`，所以 LeakSanitizer 是 `NOT RUN`，不能据此声称无泄漏。

该 run 没有部署 ARM binary 或访问真实 CAN 硬件。

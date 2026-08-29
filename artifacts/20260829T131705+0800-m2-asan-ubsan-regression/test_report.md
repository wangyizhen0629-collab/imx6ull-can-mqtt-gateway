# M2 Linux feature 宏修正后的 ASan+UBSan 回归

`-fsanitize=address,undefined` 构建通过。在
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` 和
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` 下，CTest 9/9 PASS。

LeakSanitizer 因已知 `ptrace` 环境限制为 `NOT RUN`，不能把本结果表述成无泄漏结论。
ARM 板端执行和 CAN loopback 也均为 `NOT RUN`。

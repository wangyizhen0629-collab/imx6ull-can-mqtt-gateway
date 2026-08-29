# M2 ASan+UBSan 报告

`-fsanitize=address,undefined` 构建通过。在
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` 和
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` 下，CTest 9/9 PASS。

LeakSanitizer 为 `NOT RUN`：沿用 M1 已确认的当前 `ptrace` 执行环境限制，没有把关闭
leak 检测后的结果描述为无泄漏结论。ARMv7 和 i.MX6ULL 测试也均为 `NOT RUN`。

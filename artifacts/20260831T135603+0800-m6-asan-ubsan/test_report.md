# M6 ASan + UBSan 回归

ASan+UBSan fresh build 和沙箱外全量 CTest **13/13 PASS**，日志中没有 sanitizer 报告。

LeakSanitizer：**NOT RUN**。原因是当前命令执行环境受 `ptrace` 约束，因此使用
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`；需要在不受该限制的环境另建 run 才能
产生可信的 leak 结论。

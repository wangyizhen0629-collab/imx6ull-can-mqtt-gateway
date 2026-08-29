# M2 最终 ASan+UBSan 报告

总 deadline 边界修正后的 `-fsanitize=address,undefined` 构建通过。在关闭已知无法运行的
leak 检测、启用 ASan/UBSan halt-on-error 后，CTest 9/9 PASS。

LeakSanitizer、ARMv7 和 i.MX6ULL 测试均为 `NOT RUN`；没有把本结果写成无泄漏或板端
结论。

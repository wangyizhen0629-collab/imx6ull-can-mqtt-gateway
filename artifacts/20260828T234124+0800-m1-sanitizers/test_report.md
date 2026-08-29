# M1 sanitizer 环境限制记录

- 结果：**FAIL - environment limitation**
- ASan + UBSan 构建：PASS
- CTest：0/8；8 个测试均在退出时报告同一个 LeakSanitizer 致命错误：当前执行环境
  处于 `ptrace` 下，LSan 无法运行。
- 代码缺陷结论：未观察到；本 run 不能证明无泄漏，也不能把环境中止写成测试通过。
- 后续：保留本 run，另建 run 关闭 `detect_leaks`，继续验证 ASan + UBSan。
- LeakSanitizer：**NOT RUN** —— 需要不受 `ptrace` 限制的执行环境。
- 硬件/板端/CAN/MQTT：**NOT RUN** —— 不属于 M1。

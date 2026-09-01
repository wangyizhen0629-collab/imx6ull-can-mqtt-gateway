# M8 ASan+UBSan test report

- 配置/构建：PASS。
- flags：`-fsanitize=address,undefined -fno-omit-frame-pointer -g`。
- `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`。
- `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`。
- M8 CTest：2/2 PASS，无 sanitizer 报告。
- 受限沙箱全量 CTest：16/17 PASS；唯一失败为既有 SocketCAN 测试无法创建
  `PF_CAN` socket，不是 sanitizer 内存/未定义行为报告。
- 沙箱外全量复测：NOT RUN——执行审批未获用户在知情后的明确授权。
- LeakSanitizer：NOT RUN——`detect_leaks=0`，当前执行环境处于 ptrace 管理下。
- Broker/external-loop 集成：本 run 未启动 Broker。

结论：M8 离线专项 ASan+UBSan 通过；真实 MQTT、目标板与硬件行为未由本 run 验证。

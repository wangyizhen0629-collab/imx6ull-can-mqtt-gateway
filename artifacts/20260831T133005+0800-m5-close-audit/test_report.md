# M5 最终关闭审计

结论：**PASS / M5 MET**。本审计只汇总已经实现并实际执行的 M5 证据；没有实现或测试
M6 及后续功能。

## 当前源码回归

- `build/m5-host-final` 增量 warning-clean 构建 PASS；
- 沙箱内全量 CTest 为11/12，唯一失败是沙箱拒绝 `PF_CAN` socket 的既有 M2
  `test_can_receiver`；同一轮的 M5 `test_pipeline` PASS；
- 不修改 `can0`、不连接硬件，在沙箱外复跑相同 CTest 为12/12 PASS；
- 直接运行 M5 专项：111帧基准全部消费且 queue drop 0；200条故意过载中5条消费、
  195条按策略 drop、high-watermark 4；signal 15 后两个线程均完成；
- 主机最终 artifact 保存的8份 M5 源码 SHA256 与当前源码全部一致。

沙箱失败和沙箱外通过均保留，未把环境失败改写成通过。

## 板端证据链

`artifacts/20260831T132341+0800-m5-board-owner-final/` 保存项目所有者提供的两个原始
板端日志。两份原始文件 SHA256 复核通过；目标板 binary SHA256 与当前 ARM 交叉构建
产物一致。基准3694条物理输入全部 decode/queue/pop/consume 且 queue drop 为0；
过载6532次 attempt 满足2970 success + 3561 drop + 1 closed，queue high-watermark
4/4，pop/consume 与 drop/gap 计数守恒。两次均在 signal 15 后输出 post-join summary。

项目所有者确认 STM32 在进程启动后才中途开启，因此305/75次 receive timeout 是
此前无输入时的100 ms poll，不是 queue drop，也不能将完整64/69秒声称为持续111帧/s。

## 门禁与限制

M5 要求的基准 queue drop 为0、故意慢消费者过载策略和 SIGTERM 正常退出均有主机及
真实 i.MX6ULL 功能证据，退出门禁为 MET。

LeakSanitizer、板端 `can_before/after`、完整进程区间持续111帧/s、shell 精确退出码和
正确 UTC 均为 **NOT RUN**。因此不产生 CAN 错误增量、吞吐、时延、持续运行、性能或
可靠性结论。MQTT、spool、epoll 和部署编排未实现；M6 未开始。

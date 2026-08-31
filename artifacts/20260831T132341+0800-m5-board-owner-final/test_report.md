# M5 i.MX6ULL 项目所有者实测归档

结论：`PASS / M5 MET`。项目所有者把两个板端原始日志从 i.MX6ULL `/tmp` 拉回 Ubuntu
仓库 `tmp/`；本 artifact 以新 run_id 非覆盖复制并记录 SHA256，原文件没有移动或删除。
项目所有者另行提供的板端 `sha256sum` 确认运行 binary 与 M5 ARM 交叉构建产物一致：

```text
567079d01f4fb1e682a959cd01bac3709e4062f42c1f18903596dc47181d0a01
```

## 基准链路

配置为 capacity 1024、push timeout 50 ms、sink delay 0。物理 CAN 输入共接受并成功解码
3694条，全部入队、pop 和 mock sink 消费；queue drop、decode error、receive error、
sink gap/non-monotonic/invalid 均为0，high-watermark 为2。`SIGTERM` 15 后输出 post-join
summary，因此真实板端“CAN receive → DBC decode → queue → mock sink → graceful stop”
链路通过。

305次 receive timeout 不是队列失败。项目所有者确认 STM32 是在 `gatewayd` 启动后才
中途开启，timeout 对应没有 CAN 输入时的100 ms空闲 poll。该 run 只证明实际物理输入
窗口内 queue drop 为0，不描述成完整64.019秒持续111帧/s或可靠性测试。

## 故意过载

配置为 capacity 4、push timeout 0、sink delay 20 ms。6532条成功解码输入满足：

```text
2970 queue_success + 3561 queue_drop + 1 queue_push_closed = 6532 queue_attempts
2970 queue_pop = 2970 sink_consumed
3561 queue_drop = 3561 sink_gap_records
```

high-watermark 为4/4，decode/receive error、sink non-monotonic/invalid 均为0。
`queue_push_closed=1` 是 `SIGTERM` 关闭队列时一条已接收记录竞争入队的预期边界，不是
队列未关闭。信号15后正常输出 post-join summary，过载策略、计数守恒和退出均通过。

## 证据限制

- 板端 wall clock 未初始化，1970时间戳不代表真实日期；
- STM32 在进程启动后才开启，不能引用整个进程时长计算平均输入帧率；
- 本次未归档 `can_before.txt`/`can_after.txt`，不能新增 CAN error counter 增量结论；
- shell `wait` 的精确退出码未归档，只以 signal 15 后成功输出 post-join summary证明
  graceful shutdown；
- 本阶段仍未连接 MQTT、spool 或任何 M6+ 功能。

在上述边界下，M5 所需的真实板端基准零 queue drop、故意慢消费者过载策略和
`SIGTERM` 退出均有原始日志，退出门禁满足。

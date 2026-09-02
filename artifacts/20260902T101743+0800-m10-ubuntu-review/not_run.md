# NOT RUN

以下项目没有实际执行，不得推测结果：

- STM32 ST-Link烧录：`NOT RUN`。原因：本会话为Ubuntu复核点，无Windows/实物操作，且
  未取得本次精确烧录授权。条件：Windows拉取最终提交，核对固件SHA并获得烧录授权。
- 111/500/1000帧/s三档短candump：`NOT RUN`。原因：无物理CAN/STM32端点。条件：烧录
  对应profile、保存CAN前后状态与原始candump，并用当前8项回归后的分析器复核。
- 120秒板端`/proc`指标预演：`NOT RUN`。原因：binary未私有传输、部署或运行，未授权
  目标进程操作。条件：板端复核精确binary/库SHA和ABI后获得部署及进程操作授权。
- 短端到端CAN→MQTT对账：`NOT RUN`。原因：无目标板/Broker/subscriber执行路径及授权。
- 500帧/s、30分钟：`NOT RUN`。条件：短预演全部PASS并单独批准长时间外部状态操作。
- 1000帧/s、30分钟：`NOT RUN`。条件同上，且500帧/s正式run先PASS。
- 20轮、每轮至少5分钟Broker中断：`NOT RUN`。条件：两个压力run先PASS，并明确批准
  Broker控制、恢复方案和每轮证据合同。
- 111帧/s、24小时基准：`NOT RUN`。条件：前三个正式场景均经Ubuntu validator确认PASS，
  证据容量/备份/脱敏/恢复方案完成并获得24小时运行授权。
- ARM binary在i.MX6ULL部署或执行：`NOT RUN`。本run只证明交叉构建、ELF和静态输入SHA。
- LeakSanitizer：`NOT RUN`。沿用M10既有记录；当前执行环境受ptrace限制，本复核未伪装为
  leak测试。

因此没有真实吞吐、CPU/RSS、时延、恢复时长、30分钟压力或24小时稳定性结论，M10总门禁
保持`NOT MET`。

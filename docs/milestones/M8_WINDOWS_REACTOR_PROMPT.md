# Windows Codex：执行 M8 真实 Broker/目标板 reactor 门禁

请在 Windows 端当前仓库执行以下任务。先运行 `git status --short --branch`，若存在未提交
改动，不得覆盖、清理或重置；先向我报告。工作树安全后执行
`git pull --ff-only origin master`，并记录 pull 输出和 `git rev-parse HEAD`。

完整阅读根目录 `AGENTS.md`、`docs/PROJECT_SPEC.md`、`docs/PLANS.md`、
`docs/milestones/M7.md`、`docs/milestones/M8.md`、`docs/TEST_PLAN.md`，以及 M8 当前实现和
已有 `artifacts/20260901T125544+0800-m8-preflight/`、
`artifacts/20260901T125839+0800-m8-host-dev/`、
`artifacts/20260901T125959+0800-m8-reactor-recovery-not-run/`、
`artifacts/20260901T130048+0800-m8-arm-cross/`、
`artifacts/20260901T130155+0800-m8-asan-ubsan/`。先确认 M7 门禁为 `MET`；只执行 M8，
不得实现或开始 M9 及后续功能。

## 本次授权边界

当我把本提示词发送给你时，即明确批准仅为本次 M8 门禁执行以下操作：

- 在 Windows 启动和停止一个仅供本次测试使用的 Mosquitto Broker 与订阅者；
- 将已构建的 M8 ARM binary 部署到 i.MX6ULL 的非系统测试目录；
- 使用私有配置和独立持久化 spool 启动、优雅停止 `gatewayd`；
- 在确认进程命令行、可执行文件和 PID 均属于本次 run 后，对该 `gatewayd` 执行一次受控
  `SIGKILL`，随后使用同一 binary、配置和 spool 重启；
- 为恢复场景临时停止并重启本次专用 Broker。

该授权不包括修改 Windows/i.MX6ULL 网络配置、CAN 接口状态或波特率、`/etc`、init、
systemd、固件、STM32、依赖包、非本次 Broker/进程，亦不包括长时间/性能/可靠性测试。
若现有 CAN、网络、目标板登录或 Mosquitto 条件不满足，不得自行改状态或安装依赖；将
对应步骤写成 `NOT RUN`，注明原因和所需条件。

## 证据与敏感信息规则

1. 为本次实际执行创建唯一 `artifacts/<run_id>/`，不得覆盖、截断或删除任何已有证据。
2. 原始配置、真实 LAN 地址、用户名、口令和私钥只放在 Git 忽略的私有目录。提交证据
   必须脱敏；同时记录原始文件 SHA256、脱敏文件 SHA256和脱敏方法，使证据可追溯。
3. 保存每条命令或操作的本地时间、退出码和完整原始输出。截图只能作为补充，不能替代
   文本日志、JSONL、进程信息、文件哈希和计数器。
4. 不得把 `NOT RUN`、历史 M7 结果、Ubuntu 离线测试或 ARM 交叉构建表述为本次板端
   `PASS`。不得推测任何硬件、CAN、运行时间、可靠性或性能结果。

## 执行前核验

- 记录 Windows、Mosquitto client/Broker、SSH/传输工具版本和当前时间。
- 确认选用端口未被占用，专用 Broker 配置不含公有监听或真实凭据；不要停止已有 Broker。
- 核对将部署的 ARM binary SHA256 必须为
  `2c3841e6a18ea80a470bf7d2bb8deaed314fdd1a495dc8c2b5c9a4021a8a9a6b`。若不匹配，停止并
  报告，不得用另一 binary 冒充。该构建产物按仓库规则不进入 Git；Ubuntu 原始输出路径
  为仓库外构建目录 `build/m8-arm-cross/gateway/gatewayd`。若 Windows/目标板尚未取得该
  精确文件，请让我先通过私有传输交付；不得在证据中假定它已部署。
- 在板端记录 `file`、`readelf -h/-l/-d`、SHA256、`ldd`，并核验运行时
  `libmosquitto.so.1`。预期 M7 对应库 SHA256为
  `b32c8ac4defb2b2920fba2e42f263869508c42e3c1719440db37ffc8d8c2f636`；不一致时停止并
  报告，不得替换系统库。
- spool 必须位于适合断电/重启语义的持久化 ext4 测试路径，不得使用 `/tmp`、tmpfs 或
  已有 run 的 spool。记录路径类型、初始目录清单、空间和权限，但提交时脱敏真实地址。
- 记录目标板时间、CAN 接口只读状态和本次启动前相关进程。若需改变任何状态，标记
  `NOT RUN` 并报告，不得超出授权。

## 必须验证的 M8 行为

以 M7 的真实 Broker 恢复语义为基准，但必须证明本次走的是 M8 external-loop reactor：

1. 启动专用 Broker和订阅者，再启动板端 `gatewayd`；记录启动命令、PID、配置哈希、
   spool初始状态和完整 stdout/stderr。
2. 在线接收真实 CAN 后完成 MQTT publish/PUBACK；保存订阅 JSONL 和 gateway summary。
3. 停止本次专用 Broker但保持 gateway 与 CAN 不变；证明 gateway 存活、spool pending
   增长且 ACK cursor 不被错误推进。
4. 重启同一 Broker；证明 gateway 重连并将积压补传、最终 drain。
5. 再形成可核验的 pending 数据，记录 spool data/state 的大小和 SHA256；核验本次
   gateway PID 与命令行后仅对它执行一次 `SIGKILL`。保存 kill 命令/退出状态和进程消失
   证据，用同一 binary、配置、spool 重启，证明 seq 不复用、未确认数据安全重放并最终
   drain。
6. 若 `docs/PLANS.md` 和 M7 关闭证据要求 state 损坏恢复，则在本次专用 spool 副本上按
   既有方法执行，并保存损坏前后哈希、恢复日志和安全重放结果；不要污染其他 artifact。
7. 最后对核验后的 gateway PID 发送 `SIGTERM`，证明优雅退出并保存退出码和最终 summary；
   停止本次订阅者和专用 Broker，确认没有遗留本次进程。

M8 summary/快照至少应证明 `reactor_enabled=1`，且真实运行中 `epoll_waits`、
`socket_events`、`loop_read`、`loop_write` 为非零；运行覆盖 timer 周期时 `timer_events` 和
`loop_misc` 也必须为非零。保存 eventfd/wake 计数及 publish、PUBACK、reconnect、spool
计数。若字段名与实际源码略有不同，以源码字段为准建立逐项映射，不得臆造值。

对订阅 JSONL 执行仓库既有 validator：原始 QoS1 重复允许存在，但相同 seq 的内容必须
一致；去重后 seq 连续、missing 为0、effective duplicate为0。保存 validator 命令、版本、
输入 SHA256、完整输出和退出码。任何一项未满足即为 `FAIL`，不得关闭 M8。

## 完成和停止条件

- 先汇总每个必测项为 `PASS`、`FAIL` 或 `NOT RUN`，附对应证据相对路径。
- 只有 external-loop 计数、在线 PUBACK、Broker 断开/恢复补传、受控 SIGKILL 后同 spool
  恢复、订阅数据校验和优雅退出全部具有本次真实证据且通过时，才可把 M8 更新为
  `MET`。否则保持 `NOT MET`，准确记录阻塞。
- 根据真实结果更新 `docs/PLANS.md`、`docs/DECISION_LOG.md`、
  `docs/OPEN_QUESTIONS.md`、`docs/RESUME_TRACEABILITY.md` 和
  `docs/milestones/M8.md`；需要时同步 README/TEST_PLAN，但不得改写历史证据。
- 如果发现实现缺陷，只允许修复 M8，并在新的唯一 artifact 中完整重跑受影响门禁；不得
  顺手实现 M9。
- 提交前运行敏感信息检查和 `git diff --check`，逐项审阅 staged 文件，禁止提交私钥、
  凭据、真实 LAN 地址、原始私有配置或构建产物。提交 M8 证据/文档（若有 M8 修复也一并
  提交）并 push；报告 commit、push 结果、门禁结论和所有 `NOT RUN`。
- 完成 M8 后立即停止，不得开始 M9。

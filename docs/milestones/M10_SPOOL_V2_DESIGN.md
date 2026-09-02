# M10 spool v2 分段回收与有界 group commit 设计冻结

## 状态与边界

本文只冻结 M10 内的纠正性准备，不改变 M9 已通过的历史结论，也不使 M10 变为
`MET`。本轮不连接真实板端、CAN 或 Broker，不烧录固件，不运行压力或长时间测试。
旧 M10 `RelWithDebInfo` binary
`d234f2c5f0cc732fd56bc43cc2b8f59491944111b430409ca0ab5b6bb07e4fbf`
在本设计对应源码变更后立即过期，不得部署、执行或作为后续性能输入。

本设计解决两个彼此独立的问题：

1. GSP1 单文件只推进 ACK 游标、永不回收的问题；
2. 每写入一条 80 字节记录就执行一次 `fdatasync` 的同步写放大。

实现和审阅分为两个提交。提交 A 引入 v2 分段和回收，但保持每条记录同步；提交 B
才引入可配置 group commit。任何 v2 失败均向上返回明确错误并进入 fail-stop 路径，
不以覆盖未 ACK 数据、跳过损坏记录或假装成功来维持运行。

## 格式选择与 legacy 边界

- 新增显式配置 `spool_format=legacy|v2`，默认值为 `legacy`。默认值保证现有
  M7/M8/M9 的 GSP1/GST1 行为和恢复驱动不会被无声改变。
- `legacy` 继续把 `spool_path` 解释为单个 GSP1 data 文件，并使用相邻 GST1 state
  文件。它不获得分段回收或 group commit。
- `v2` 把 `spool_path` 解释为专用目录。该路径必须是新目录或只含下述 v2 已知文件
  的目录；如果路径是普通文件、出现 GSP1/GST1、出现无法识别的文件或 segment 集合
  无法安全解释，打开失败。
- 不自动探测后再选择格式，不原地改写、截断、移动或删除 legacy 文件，不提供隐式
  迁移。若将 legacy 路径误配为 v2，必须 fail closed。需要迁移时必须另立经批准、
  可回滚且有独立证据的流程；它不属于本轮。
- 本轮不得修改或清理板端现存 M9 spool。M9 已安装 binary 和现有 GSP1/GST1 数据
  仍属于原历史输入，不受本轮源码改写。

## v2 磁盘布局

`spool_path` 目录只允许以下名字：

```text
spool-v2/
  lock
  state.v2
  state.v2.tmp              # 仅原子 state 事务期间出现
  segment-00000000000000000001.gsp2
  segment-00000000000000000002.gsp2
  ...
```

- segment 从 1 开始，使用 20 位、前导零、十进制无符号编号。编号只递增，不复用。
- 每个生产 segment 固定最多 65536 条记录；每条固定 80 字节；满 segment 恰为
  5,242,880 字节。测试故障接口可缩小单次测试的滚动边界，但不得改变生产配置。
- segment 没有独立可变头，内容是连续 GSP2 记录。记录头 16 字节：偏移 0 为
  `GSP2`，偏移 4 为 little-endian `uint16 version=2`，偏移 6 为
  `uint16 size=80`，偏移 8 为 CRC32，偏移 12～15 保留为 0；其后 64 字节 payload
  与 GSP1 的字段偏移相同。
- 记录 CRC 使用 IEEE CRC-32（反射多项式 `0xedb88320`，初值和末值异或均为
  `0xffffffff`），只覆盖 64 字节 payload。magic、version、size、保留字和 CRC 也必须
  分别校验；任何非零保留字均拒绝。
- 活跃 segment 可以是 80 字节整数倍的部分文件。只有最后编号的活跃 segment 允许
  末尾出现不足 80 字节或最后一条 CRC 不完整；恢复时只截断该尾部并同步。任何内部
  CRC 损坏、非最后 segment 的部分尾、非单调 sequence 都 fail closed。

`state.v2` 固定 112 字节，使用 little-endian：

| 偏移 | 大小 | 字段 |
|---:|---:|---|
| 0 | 4 | `GST2` |
| 4 | 2 | `version=2` |
| 6 | 2 | `size=112` |
| 8 | 4 | CRC32，覆盖偏移 12～111 |
| 12 | 4 | flags，当前必须为 0 |
| 16 | 8 | generation |
| 24 | 8 | last allocated `gateway_seq` |
| 32 | 8 | sequence reservation fence |
| 40 | 8 | last ACKed `gateway_seq` |
| 48 | 8 | next batch sequence |
| 56 | 8 | ACK segment |
| 64 | 8 | ACK byte offset |
| 72 | 8 | current write segment |
| 80 | 8 | current write byte offset |
| 88 | 8 | segment record limit，生产值 65536 |
| 96 | 8 | reserved，必须为 0 |
| 104 | 8 | reserved，必须为 0 |

state CRC 算法与记录相同。`generation` 每次成功提交 state 加一，不能回绕；
`next batch sequence` 从 1 开始。ACK/write offset 必须是 80 的整数倍且不超过当前
segment 上限。首次打开空目录时，在允许任何 segment 写入前先持久化 generation 1
的初始 state；因此“有 segment 却没有任何有效 state”不是可猜测恢复情形，必须失败。

## sequence 永不复用

`last allocated gateway_seq`、`last ACKed gateway_seq`、`next batch sequence` 和
单调 segment 编号都保存在 state，而不是从仍存在的数据文件单独推导。全部旧 segment
删除且 pending 为 0 后，state 和 `lock` 仍保留；重启从 state 高水位继续，不能回到 1。

group commit 中若只在数据 flush 时更新 sequence，掉电可能使未同步记录的 sequence
被复用。为避免这一点，提交 B 在接受一个新同步组的第一条记录前，先用 state 事务
持久化一个不超过 `spool_sync_records` 的 `sequence reservation fence`。运行时实际记录
仍连续使用保留区间；正常 flush/关闭把 `last allocated` 更新为实际末条，并把 fence
收缩到实际值。若在保留后、数据同步前崩溃，恢复从 fence 后继续：允许出现明确的
sequence 缺口，但不复用已经发放给失败同步组的编号。该缺口属于下述允许掉电损失窗口，
不能描述为零丢失。严格模式 `sync_records=1` 不产生额外保留空洞。

state 与 segment 扫描结果必须相互约束：扫描所得最大实际 sequence 不得大于有效 fence；
不得小于已经 ACK 的 sequence；ACK 后仍需读取的 segment 必须连续。state 已确认删除的
较老 segment 可以缺失，也可能因目录元数据尚未落盘而在崩溃后重新出现；后者只可在
完整校验并确认全部已 ACK 后再次删除。

## 精确写入、同步、ACK 和删除顺序

### 首次创建

1. 创建专用目录；打开目录 fd。
2. 创建并锁定 `lock`；新建目录项后同步目录。
3. 写 `state.v2.tmp`，`fdatasync(tmp)`，关闭 tmp。
4. `rename(state.v2.tmp, state.v2)`。
5. `fsync(spool directory)`；此后才允许 append。

### append 与 segment 滚动

1. 校验 sequence、容量和当前失败状态。达到 `spool_max_bytes` 时直接返回
   `GATEWAY_ERROR_CAPACITY`，上层记录 spool error；不得删除/覆盖未 ACK 数据。
2. group 模式若没有覆盖本条 sequence 的有效 reservation，先按 state 临时文件事务
   持久化 fence；事务顺序与下面 state 提交相同。
3. 如目标 segment 尚不存在，用 `O_CREAT|O_EXCL` 创建，随后 `fsync(directory)`，再写
   数据；不允许复用同编号旧文件。
4. 在确定 offset 执行完整 80 字节 `pwrite`。部分写或错误使实例 fail-stop；重启仅可按
   “活跃 segment 尾部”规则恢复。
5. 严格模式立即 `fdatasync(segment)`；group 模式累计未同步条数，并在记录数阈值、
   时间阈值、segment 滚动、publish 前或正常关闭时执行 `fdatasync(segment)`。
6. 数据同步成功后，用新 generation 写完整 `state.v2.tmp`，`fdatasync(tmp)`，关闭，
   `rename` 为 `state.v2`，再 `fsync(directory)`。只有此步完成，严格 append 或一次 group
   flush 才报告持久化成功。
7. 记录恰好填满 segment 时，提交的 write cursor 指向下一个单调 segment 的 offset 0；
   下一个文件延迟到真正 append 时创建。

任何记录进入 MQTT publish 候选集之前，`gateway_spool_prepare_batch` 必须先完成步骤
5～6；同步失败则不读取、不编码、不 publish。

### 恢复时接纳持久state游标之后的完整记录

加载GST2后必须先保存其中原始的`write_segment`和`write_offset`，扫描过程不得用文件长度
静默覆盖这两个持久游标。若原write segment文件含有游标之后、CRC和sequence均有效且不
超过reservation fence的完整记录，恢复保持“同步并接纳”语义，精确顺序为：

1. `openat(segment, O_RDWR)`、`fstat`、`pread`并校验完整记录，关闭扫描fd；若有半条尾部，
   先`ftruncate`到最后完整边界并`fdatasync`后关闭。
2. 重新`openat`原持久write segment为`O_RDWR`，对整个segment执行`fdatasync`并关闭。
   同步或关闭失败立即使open失败；不得更新磁盘state。
3. 只有步骤2成功后，才在内存中把last allocated和write offset推进到扫描所得完整边界。
   若完整边界恰好等于segment容量，内存write cursor滚到下一单调segment的offset 0；此时
   不创建下一个segment文件。
4. 以新generation执行state事务：`openat(state.v2.tmp, O_TRUNC)`、完整`pwrite`、
   `fdatasync(tmp)`、`close(tmp)`、`renameat(tmp,state.v2)`、`fsync(directory)`。

因此恢复所需segment sync失败时，原state的generation、last allocated、write segment/
offset和reservation fence保持逐字节不变；随后无故障reopen可重复扫描、先同步再接纳。
若state落后且segment记录已经由先前flush同步，恢复仍保守地再次`fdatasync`，不依靠对
崩溃前页缓存状态的猜测。

### PUBACK、state 和回收

1. 只有匹配当前单 in-flight publish 的 PUBACK 才计算新的 ACK cursor、last ACK seq 和
   next batch seq。
2. 若 ACK 到达当前部分 segment 的实际写末端且 pending 将变为 0，先把 write/ACK
   cursor 一起滚到下一个单调 segment 的 offset 0，使该部分 segment 也成为完全 ACK
   的旧 segment。
3. 用 `state.v2.tmp` 完整写入新 generation、ACK/write cursor 和 sequence 高水位；执行
   `fdatasync(tmp)`、关闭、`rename(tmp,state.v2)`、`fsync(directory)`。
4. 只有步骤 3 全部成功后，才逐个 `unlink` 编号小于新 ACK segment、且逐条校验确认
   不含未 ACK 记录的 segment。不得删除 ACK cursor 所在的部分 segment，也不得覆盖
   任意未 ACK 记录。
5. 所有预定删除成功后再次 `fsync(directory)`。任一删除或最终目录同步失败都向上返回
   错误并 fail-stop；state 已推进时重启可依据持久 ACK 安全地重新完成幂等删除。

`physical_bytes` 是当前所有 GSP2 segment 的字节数，不含 state/lock 元数据；
`pending_bytes=pending_records*80`；`segment_count` 是现存 segment 数；
`segments_reclaimed` 统计本进程成功删除的 segment；`sync_count` 统计成功的
`fdatasync/fsync` 调用，`sync_failures` 统计失败调用。另保留 append/replay/tail/state/
corruption 等历史统计。

## 各阶段崩溃恢复矩阵

| 崩溃点 | 重启后的唯一允许结果 |
|---|---|
| 初始 tmp 写入/同步前后、rename 前 | 无 segment 时删除或覆盖残留 tmp，重新创建初始 state；绝不猜测已有数据 |
| 初始 state rename 后、目录 fsync 前 | ext4 可能呈现旧目录状态；无 segment则重建，出现 segment 而无有效 state则 fail closed |
| reservation state 写入/同步/rename 前 | 旧 fence 生效，本条 append 尚未开始；sequence 可按旧 state 继续 |
| reservation rename 后、目录 fsync 前 | 只接受旧 state 或完整新 state；若新 fence 可见，恢复从 fence 后继续，允许缺口但不复用 |
| segment 创建前后、首次目录 fsync 前 | 空的新文件可删除后重建；非空文件必须有有效旧 state 才可扫描，否则 fail closed |
| 80 字节 pwrite 前 | state 和 segment 都保持上一持久边界 |
| pwrite 途中或完成但 segment sync 前 | 仅最后活跃 segment 尾部可被截断到最后有效80字节边界；若扫描看到state write cursor之后的完整记录，必须先`fdatasync`原write segment，成功后才可接纳并提交state；sync失败保持旧state并fail closed |
| 恢复segment sync成功后、纠正state tmp前 | 完整记录现已满足data-before-state；旧state仍有效，重复崩溃后再次扫描并同步即可 |
| segment sync 后、state tmp 前 | 扫描到完整记录；在有效 fence 内按上述恢复顺序再次同步后安全纳入，ACK/batch仍取旧state |
| state tmp 写入或 fdatasync 前后、rename 前 | 忽略/清理 tmp，使用旧 state并按已同步 segment前向协调；不倒退 ACK |
| state rename 后、目录 fsync 前 | 只接受 CRC 完整的旧或新 state；二者都不引用未同步数据。无法满足扫描约束则 fail closed |
| ACK state 提交前任一点 | 旧 ACK 生效；对应 batch 会重放，允许 QoS 1 raw duplicate |
| ACK state rename/目录 fsync 成功后、删除前 | 新 ACK 生效；不再重发已 ACK 记录，重启继续删除完全 ACK segment |
| 单个旧 segment 删除前后、最终目录 fsync 前 | 已删除文件可能消失或重现；有效 ACK state证明其全部已 ACK，重启幂等删除；未 ACK segment 不受影响 |
| 最终目录 fsync 后 | 回收完成，physical bytes 和 segment count 反映删除结果 |

内部 CRC 损坏、缺失/乱序的未 ACK segment、同编号非法名字、两个无法判定先后的有效
state、state 指向不存在的未 ACK 数据，均不允许“尽量继续”，而是 fail closed。

## 容量上限

- 新增 `spool_max_bytes`，只用于 v2，默认 268,435,456 字节（256 MiB）。允许范围为
  5,242,880 字节（一个生产 segment）到 68,719,476,736 字节（64 GiB）。上限不必是
  80 的整数倍；append 仍按完整 80 字节记录检查，绝不越过该字节上限。
- append 前按当前 `physical_bytes + 80` 判断。可以先完成已经由持久 ACK 授权的幂等
  回收，但不得为了腾空间 ACK、丢弃或覆盖最老的未 ACK 数据。
- 到达上限返回明确 range 错误，由 MQTT sink 增加 spool error、记录日志并停止接收；
  不使用环形覆盖，不静默 drop。
- 默认值仅是保守的离线候选，依据是当前已知目标根 ext4 总量约 1.43 GiB、可用约
  636 MiB；它不是板端容量验证结论，Windows/板端复核前仍可显式下调。

## group commit 和持久性边界

- `spool_sync_records` 默认 1，范围 1～65536。值 1 是 legacy/严格语义，不无声弱化
  M7/M8/M9 的每条记录同步边界。
- `spool_sync_interval_ms` 默认 1000，范围 1～60000；在 `sync_records=1` 时记录阈值
  总是先触发。
- M10 后续测试候选明确为 `spool_sync_records=128`、
  `spool_sync_interval_ms=1000`，以先到者为准。它不是尚未测量的性能最优值；选取依据
  只是把每条同步降为有界批量，同时将低流量/离线驻留窗口限制在 1 秒触发点。
- append 路径达到记录阈值立即 flush；consumer idle poll 按单调时钟检查时间阈值；
  持续输入路径也检查；Broker 离线不会绕过这两种触发。
- publish 前强制 flush，正常 `gateway_mqtt_sink_flush` 和 spool close 强制 flush；任何
  sync/state 失败向调用者传播。析构中的兜底 flush 无法改变 `void` 接口，因此正常主
  流程必须先调用可返回错误的 flush，并把失败作为进程失败。
- 候选 128/1000 下，成功返回但尚未持久化的 append 最多 127 条；第 128 条会在返回前
  同步，但若掉电发生在该次调用的写入和 `fdatasync` 完成之间，磁盘上最多 128 条完整
  新记录仍可能一起丢失。时间边界是从本组首条 append 到 1000 ms 阈值触发，两个阈值
  以先到者为准。reservation 确保这些 sequence 不复用，但可能留下最多 128 个序号的
  缺口。调度停顿、内核或存储设备
  不兑现 flush 语义不在应用层可证明的绝对时间界内，所以不能声称真实掉电零丢失。
- 一旦 segment `fdatasync` 成功但随后 state 提交失败，实例 fail-stop；重启扫描可在
  已持久 fence 内纳入完整记录。应用不继续 publish 或覆盖不确定数据。

group commit只修正了调用和数据顺序，不构成在线写放大已改善的实测结论。当前
pending为0时，ACK会把部分活跃segment的write/ACK cursor滚到下一编号并删除该segment；
1秒batch可能表现为每秒创建、同步和删除一个小segment。该元数据写入是否抵消group
commit收益，必须留待经批准的板端120秒预演同时量化`spool_syncs`、segment create/delete
以及块设备写入增量；本轮不得推测。

## 必须验证的离线测试

提交 A 覆盖 segment 滚动/跨段顺序、全段及 pending=0 回收、删除后 sequence/batch
不复用、活跃尾部、内部 CRC、缺失/乱序、容量上限以及 append/sync/state rename/delete
故障注入。提交 B 再覆盖记录/时间阈值、离线定时 flush、publish 前强制 flush、正常关闭、
sync 失败传播和跨多个 segment 的离线积压/顺序 drain 状态机。

纠正提交C必须再构造不调用用户态close的子进程`_exit`场景，使完整记录位于持久state
write cursor之后；分别覆盖部分segment和恰好填满segment。恢复segment sync故障注入时
open必须失败且state逐字节不变，随后无故障reopen必须先同步再推进generation/cursor并
按sequence顺序读回记录。

现有 M7/M8 的 GSP1 SIGKILL、重连、GST1 损坏安全重放和 duplicate validator 必须全量
回归；它们验证兼容路径，不得改写为 v2 真实 Broker 证据。所有真实硬件、板端、CAN、
Broker、500/1000 帧每秒、20 轮断网和 24 小时测试本轮一律 `NOT RUN`。

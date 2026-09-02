# Windows端M10真实硬件续跑清单

本清单用于Windows clone、STM32CubeMX/Keil/ST-Link、真实i.MX6ULL、物理CAN和Windows
专用Mosquitto端点之间的M10续跑。Ubuntu已在提交`691c3bd`完成M10离线工具、主机回归、
sanitizer和ARMv7交叉构建；真实压力、断网和24小时门禁仍是`NOT RUN`，M10总门禁仍为
`NOT MET`。

**生成或转发本清单不等于批准硬件操作。** 烧录STM32、停止或启动目标进程、启停Broker、
修改CAN/网络/`/etc`/init、部署binary以及任何长时间测试，仍须在实际操作前取得项目所有者
对精确动作和恢复方案的明确批准。

## 0. 全程判定规则

- 每完成一个方框再进入下一项；任一“停止条件”命中时立即停止，不得用猜测值补齐。
- 尚未开始的真实测试写`NOT RUN`并记录原因和所需条件；已经开始但提前终止、证据缺失或
  判定不合格写`FAIL/INCOMPLETE`，不得改写成`NOT RUN`。
- 四个正式场景必须使用四个新的唯一`artifacts/<run_id>/`，互不复用spool、topic、
  device ID、日志或摘要。失败重跑也必须换新run ID。
- 不覆盖、截断、删除或重新生成已有证据。原始私有配置和真实地址只放各run下已忽略的
  `private_raw/`；公开证据保存原始文件大小、SHA256、脱敏方法和脱敏后SHA256。
- 四个场景必须依次通过：`stress_500`、`stress_1000`、`broker_interruptions`、
  `baseline_24h`。前一项未通过时不启动后一项。
- M10完成后立即停止，不开始任何后续Milestone。

## 1. Windows仓库预检

- [ ] 在Windows clone记录以下命令的完整输出和退出码：

  ```powershell
  git status --short --branch
  git fetch origin
  git pull --ff-only origin master
  git rev-parse HEAD
  git rev-parse origin/master
  git merge-base --is-ancestor 691c3bd HEAD
  ```

- [ ] `HEAD`与`origin/master`必须相同，且必须包含`691c3bd`。读取根目录`AGENTS.md`、
  `docs/PROJECT_SPEC.md`、`docs/PLANS.md`、`docs/TEST_PLAN.md`、
  `docs/milestones/M9.md`、`docs/milestones/M10.md`和本清单。
- [ ] 确认M9仍为`MET`、M10仍为`NOT MET`。保存Windows、Git、PowerShell、Keil、
  STM32CubeMX、ST-Link、Mosquitto client/Broker和SSH工具版本。
- [ ] 记录Windows正确UTC/本地时间；目标板时钟若仍不正确，只记录`/proc/uptime`、
  boot ID和Windows端时间线，不得把目标板1970时间描述为正确UTC，也不得擅自改时钟。

停止条件：存在不能安全保留的本地改动、pull不是fast-forward、HEAD不一致、M9证据被破坏，
或M9不再满足门禁。不得执行`reset --hard`、`clean`或删除未跟踪证据；先报告。

## 2. 正式测试前必须关闭的准备缺口

### 2.1 冻结STM32压力profile

当前仓库STM32固件只按10/100/1000 ms发送`0x100/0x101/0x102`，总速率111帧/s；它不能
直接证明500或1000帧/s。正式烧录前必须完成：

- [x] 为111、500、1000帧/s冻结明确的每ID速率、调度算法、运行边界和选择方式；三类
  标准DLC-8帧、各自Rolling Counter、XOR Checksum及既有DBC语义必须保留。
- [x] 压力profile必须基于整数、可复现调度；不得用Windows sleep或平均估算冒充物理
  速率。必须说明发送失败时counter和调度如何处理。
- [x] 业务修改优先位于`USER CODE BEGIN/END`。不得加入RTOS、USB协议、GUI、网络栈、
  MQTT、文件系统或其他超出模拟ECU范围的组件。
- [x] Keil对每个将烧录的profile执行完整rebuild，保存0 error/0 warning原始日志、固件
  SHA256、`.ioc`/工程/关键源码SHA256和实际target名称。
- [x] 现有`tools/protocol/check_stm32_candump.py`固定为111帧/s的60秒合同。必须先扩展它
  或新增M10专用分析器，使其能从真实candump按profile输出每ID计数、实际持续时间、速率、
  counter gap、DLC/checksum和意外ID；为新分析器增加无硬件回归。
- [ ] STM32源码、分析器和测试先形成单独M10准备提交并push，然后停止，由Ubuntu拉取并
  复核diff、warning-clean/CTest和分析器回归。Ubuntu明确放行后才能烧录或长跑。

建议先提交“profile设计表”供项目所有者确认，不要在未确认每ID分配和边界语义时自行
选择一个恰好相加为500/1000的数字组合。

Windows准备结果：项目所有者已确认`M10_STM32_PROFILE_DESIGN.md`；分析器回归7/7、
三个ARMCC 5.06u6 target完整rebuild 0 error/0 warning及产品hash见
`artifacts/20260902T094824+0800-m10-windows-profile-prep2/`。本节最后一项“提交并push后
停止”仍待本次提交完成；Ubuntu复核、烧录和真实短测均未开始。

停止条件：500/1000仅是推算值；高负载时发送失败；三类ID任一为0；counter/checksum
合同改变；Keil有warning/error；分析器不能从原始数据独立复算。此时不得开始正式run。

### 2.2 冻结被测gateway binary

- [ ] Ubuntu现有M10 ARMv7 binary SHA256为
  `7bb1d7299eac43d5a7a9b8f52981652c6ed3e3f3b29567ff74a1abc5f2b3edef`，它是Debug构建，
  尚未部署或上板运行。项目所有者必须二选一并记录：
  1. 明确接受所有M10数值只代表这个精确Debug binary；或
  2. 先回Ubuntu生成并验证指定的Release/RelWithDebInfo binary，再以新SHA替换被测输入。

已选择方案2：`RelWithDebInfo`。Ubuntu尚未生成、验证或冻结新SHA，因此本项保持未完成，
现有Debug SHA不得用于正式M10性能run。
- [ ] binary必须经私有传输取得；不得从Git artifact猜测或用M9旧binary替代。先在非系统
  staging目录复核`sha256sum`、`file`、`readelf -h/-l/-d`和无RPATH/RUNPATH。
- [ ] 板端实际加载的`libmosquitto.so.1`真实文件SHA256应与已冻结值
  `b32c8ac4defb2b2920fba2e42f263869508c42e3c1719440db37ffc8d8c2f636`一致。不同则停止，
  不得擅自替换系统库。

停止条件：binary/build type未选择、精确文件不可得、板端SHA/ABI/解释器/NEEDED/库映射
不匹配。不得部署或开始测量。

### 2.3 冻结运行边界和证据容量

- [ ] 先用短预演证明一个可复现的开始/结束协议，使gateway接受计数、每ID CAN计数、
  MQTT去重后unique record和最终drain能够在同一时间窗内对账。不得在正式run后再改变
  截取窗口以迎合结果。
- [ ] 每个run使用全新ext4 spool；禁止`/tmp`、tmpfs、已有spool或共享state。固定80字节
  entry意味着111帧/s运行86400秒时，仅`spool.data`理论增长约767232000字节；还必须
  容纳candump、日志、state和安全余量。
- [ ] 正式24小时前，目标持久化文件系统至少保留4 GiB可用空间，Windows私有原始证据盘
  至少保留10 GiB；把阈值和停止余量写进run计划。若实际证据格式估算更大，应提高而不是
  降低阈值。
- [ ] 24小时原始candump/subscriber可能不适合进入Git。预先确定：原始文件保留在
  `private_raw/`或受控外部存储，公开artifact提交分析器、摘要、首尾样本、文件大小、
  SHA256和脱敏追踪。没有可靠保留和复核方案时不得开始24小时run。
- [ ] `collect_proc_metrics.sh`只接受预先给定的样本数。每个场景必须先冻结验收时间窗和
  样本数；CSV至少`duration_seconds + 1`行OK样本，覆盖完整时间窗，最大采样间隔不超过
  2.5秒。

停止条件：无法精确对账、存储低于预设值、文件系统不是持久ext4、证据回收会丢原始字节，
或采集器无法稳定绑定同一PID/starttime/exe。

## 3. 获取一次明确的外部状态授权

在操作前向项目所有者列出并获得书面批准，至少覆盖：

- [ ] 最多三个已核验STM32 profile的Keil/ST-Link烧录及烧录后短测；
- [ ] 受控停止M9 supervisor管理的child、从非系统目录启动/停止M10手工测试进程，测试后
  恢复原M9服务并核验1 supervisor/1 child；
- [ ] 启动/停止Windows本次专用Broker和subscriber，以及断网run中精确20轮Broker
  stop/start；
- [ ] 两个至少30分钟压力run、一个至少100分钟且含drain时间的断网run、一个至少24小时
  基准run；
- [ ] 每个动作的恢复方案、目标目录、端口、预计最长持续时间和磁盘停止阈值。

本授权不应默认包含修改Windows/目标网络、CAN bitrate或up/down状态、`/etc`、inittab、
依赖包、系统Broker或其他进程。若任何一项确实需要改变，另列精确动作和原状态恢复方案，
重新获取批准。

停止条件：批准含糊、目标不唯一、恢复方案未知、需要新增未获批外部状态修改。

## 4. 通用硬件预演

- [ ] 为预演创建唯一run；在任何写操作前保存Windows/目标信息、boot ID、`df -T/-k`、
  `mount`、相关进程、端口、binary/库SHA，以及`ip -details -statistics link show can0`。
- [ ] 只读核对`can0`仍是500000 bit/s、loopback off、UP/ERROR-ACTIVE且berr为0/0。若状态
  不满足，停止并请求单独批准；不得顺手恢复。
- [ ] 专用Broker不得占用或停止现有Broker。配置只监听本次所需接口/端口；真实地址和
  凭据不进入源码或公开artifact。
- [ ] 每个run使用独立device ID、topic、配置、spool和subscriber输出。公开配置须脱敏，
  同时记录原始/脱敏SHA256映射。
- [ ] 在获批后，先安全核验M9 child PID/comm/exe/cmdline，再用既有init脚本执行受控stop；
  supervisor保持存活且显示disabled。不要修改`/etc`或inittab。
- [ ] 用测试目录中的binary和私有配置启动手工`gatewayd`，保存控制脚本、PID、starttime、
  exe、cmdline、映射库和stdout/stderr。证明SSH窗口断开不会误杀长跑进程。
- [ ] 从仓库精确版本部署`collect_proc_metrics.sh`到测试目录，以本次PID文件和实际exe调用。
  先执行至少120秒预演，确认全部样本为`OK`、PID/starttime不变且最大间隔不超过2.5秒。
- [ ] 对111、500、1000三个STM32 profile分别完成短candump；用M10分析器验证实际速率、
  三ID计数、counter、DLC、checksum、意外ID和CAN error frame。
- [ ] 用一组短端到端流量证明gateway summary、candump、subscriber和spool最终drain可以
  按预定边界对账；同时证明subscriber已订阅成功后gateway才允许连接/重连。
- [ ] 预演完成后优雅停止手工gateway，恢复M9服务，核验进程、CAN和Broker最终状态，并为
  预演artifact生成manifest。预演不能替代任何正式场景。

停止条件：出现发送失败、CAN error、counter gap、queue drop、MQTT missing/effective
duplicate、spool error、意外进程重启、采样缺失、订阅时序无法证明或无法最终drain。

## 5. 正式场景A：500帧/s，至少1800秒

- [ ] 创建全新`stress_500` run、spool、topic、配置和私有原始目录；给该artifact路径添加
  `.gitattributes`规则`-text -whitespace`，避免Windows/Ubuntu换行转换原始证据。
- [ ] 烧录并按SHA/Keil target/ST-Link日志确认已批准的500帧/s profile；短candump再次
  确认profile身份。
- [ ] 保存测试前CAN统计、磁盘、Broker、subscriber和进程状态。先启动专用Broker和
  subscriber并证明订阅成功，再启动手工gateway和`/proc`采集器。
- [ ] 对一个预先冻结的完整时间窗连续采集至少1800秒。若摘要使用1800秒，采集器至少要有
  1801个覆盖该窗的OK样本；使用更长`duration_seconds`时，CAN下限和样本数也随实际时长
  增加，不能仍按1800秒填报。
- [ ] 停止输入窗后保持gateway/Broker/subscriber直到spool pending、queue和in-flight均为
  0，再优雅停止本次gateway；恢复M9服务并核验1 supervisor/1 child。保存最终summary、
  CAN统计、恢复状态和所有退出码。
- [ ] 从原始证据生成每ID计数/gap、MQTT raw/unique/missing/duplicate、queue、spool、
  process和`run_summary.json`，在Ubuntu执行第9节validator。

单场景通过要求：总CAN帧数至少`500 * duration_seconds`，三ID计数之和严格等于总数，
MQTT unique严格等于总数；CAN gap/error/RX error/drop/overrun、queue drop、MQTT missing/
effective duplicate、spool error/corruption和进程退出均为0；`/proc`覆盖完整且validator
exit 0。QoS 1 raw duplicate允许存在但必须报告。

停止条件：运行中一旦发现身份变化、存储阈值、采集缺失、CAN/MQTT/spool错误或非计划
Broker中断，保留现场并标`FAIL/INCOMPLETE`；不得进入1000帧/s。

## 6. 正式场景B：1000帧/s，至少1800秒

只有`stress_500`的独立validator为PASS后才执行。本节重复第5节的全部隔离、时序、采集、
drain和恢复步骤，但必须使用全新run并烧录经核验的1000帧/s profile。

单场景通过要求相同，帧数下限改为`1000 * duration_seconds`。CAN总线不是ERROR-ACTIVE、
berr或RX drop增加、STM32报告发送失败、实际总速率未达到1000帧/s，均为FAIL。不得用短时
突发的峰值或平均估算替代完整时间窗计数。

停止条件：本场景validator非0或任一证据不完整；不得进入20轮断网。

## 7. 正式场景C：20轮Broker中断

只有两个压力场景均PASS后执行。烧录并核验111帧/s基准profile，创建全新
`broker_interruptions` run。

- [ ] 私有配置冻结`mqtt_reconnect_interval_ms`，并在短预演中证明每次Broker恢复后可以
  让subscriber先完成订阅，再等gateway下一次重连。无法证明该顺序时不得开始20轮。
- [ ] 一个gateway进程、一个spool和一份连续`/proc` CSV覆盖全部20轮；期间PID、starttime
  和exe不得变化。每轮开始前必须已drain到0。
- [ ] 对第1～20轮逐轮执行：记录Windows单调时间基准；停止**本次专用Broker**；保持
  STM32、gateway和CAN运行至少300秒；证明ACK cursor冻结而spool pending增长；在下一次
  gateway重试前启动同一Broker并确认subscriber订阅成功；记录Broker ready到首条补传的
  `reconnect_latency_ms`；等待最终drain为0后才进入下一轮。
- [ ] 任何一轮少于300秒、编号不连续、订阅者错过重连、未drain就进入下一轮，整次run均
  为FAIL，不能补做第21轮来替代。
- [ ] 20轮后继续保持链路正常直至最终drain，优雅停止本次gateway，并恢复M9服务和专用
  Broker状态。`run_summary.json`必须含恰好20个cycle，`reconnects >= 20`。

validator还要求`duration_seconds >= 6000`，但实际总时长应包含完整连续验收窗，不能只把
20个离线窗口相加后忽略在线drain阶段。报告重连/首条补传时延nearest-rank P50/P95/最大值；
当前没有冻结产品阈值，只能报告实测值。

停止条件：任一非计划进程退出、CAN gap/error/drop、queue drop、spool错误、MQTT missing/
effective duplicate、采样异常或循环时序证据缺失。不得进入24小时基准。

## 8. 正式场景D：111帧/s、至少86400秒

只有前3个场景均由Ubuntu validator确认PASS后才开始。使用全新`baseline_24h` run、全新
ext4 spool、111帧/s profile、独立topic/device ID和已核验binary。

- [ ] 开始前再次确认目标可用空间至少4 GiB、Windows证据盘至少10 GiB、稳定供电、不会
  自动睡眠/更新/重启，并写明空间停止余量。
- [ ] 专用Broker和subscriber先就绪，随后启动gateway、candump和
  `collect_proc_metrics.sh --samples 86401 --interval-sec 1`；采集器绑定同一PID文件和exe。
- [ ] 连续86400秒内不得安排Broker中断、进程restart、重新烧录、网络/CAN切换或日志
  截断。可每小时保存只读checkpoint：Windows/目标uptime、PID/starttime、磁盘、spool
  大小、CAN统计和subscriber文件大小；checkpoint本身不得改变被测状态。
- [ ] 运行中若目标剩余空间低于预设停止余量，应保留证据并受控停止，结果为
  `FAIL/INCOMPLETE`；禁止删除或压缩正在使用的spool/log以强行继续。
- [ ] 满86400秒后结束输入验收窗，等待最终drain为0，完成最终CAN/MQTT对账和`/proc`采集，
  再优雅停止gateway并恢复M9服务。保存恢复后的1 supervisor/1 child、CAN和Broker状态。

单场景通过要求：总CAN帧数至少`111 * duration_seconds`并与MQTT unique严格相等；三ID
counter gap、CAN error/RX error/drop/overrun、queue drop、MQTT missing/effective duplicate、
spool error/corruption和进程退出均为0；86401个或更多OK样本覆盖至少86400秒，最大间隔
不超过2.5秒；Ubuntu validator exit 0。提前数分钟或用86400行代替86401行都不通过。

## 9. 每个run的Ubuntu独立校验

每个正式run完成后先停止Windows端推进，把公开artifact和可核验的私有原始证据索引交给
Ubuntu。先检查原始SHA256、脱敏追踪和manifest，再用尚不存在的新输出名执行：

```sh
python3 -B tools/metrics/report_proc_metrics.py \
  --input artifacts/<run_id>/proc_metrics.csv \
  --summary-json artifacts/<run_id>/proc_report.json \
  --report-md artifacts/<run_id>/proc_report.md

python3 -B tools/metrics/validate_m10_run.py \
  --run-summary artifacts/<run_id>/run_summary.json \
  --proc-metrics artifacts/<run_id>/proc_metrics.csv \
  --output-json artifacts/<run_id>/m10_gate.json \
  --report-md artifacts/<run_id>/m10_gate.md
```

`run_summary.json`必须使用`gateway.m10.run.v1`，字段以
`tools/metrics/test_validate_m10_run.py`的`base_summary()`为结构模板，但所有值必须从本次
真实证据计算。至少包括：

- `run_id/scenario/environment/duration_seconds/target_rate_fps`，其中environment严格为
  `imx6ull-physical`；
- CAN总数、三个ID各自计数和gap、error frame及RX error/drop/overrun增量；
- queue drop；MQTT raw batch、unique/missing/raw duplicate/effective duplicate/reconnect；
- spool最大pending、进程退出次数；
- 非断网场景的`broker_interruptions`为空数组；断网场景为恰好20个连续cycle及真实时延。

validator或报告器非0时保留stdout/stderr和退出码，该场景为FAIL。修复只能限于M10，且受
影响场景必须使用新run完整重跑；不得编辑summary迎合validator。

## 10. 证据定稿、文档和最终停止

- [ ] 每个run至少保存：命令/操作时间线、git commit、主机/板端信息、授权记录、公开脱敏
  配置、原始/脱敏SHA追踪、binary/库/固件哈希、Keil/ST-Link日志、CAN前后统计、candump
  分析、gateway日志/summary、subscriber/Broker分析、spool快照、`proc_metrics.csv`、
  `run_summary.json`、报告器和validator完整输出及恢复状态。
- [ ] 对公开artifact逐个添加精确`.gitattributes`路径规则`-text -whitespace`，再生成一次
  `artifact_manifest.sha256`并自检。manifest定稿后不再改该run；需要补证据时使用新run。
- [ ] 运行敏感信息扫描、JSON解析、manifest复核和`git diff --check`；逐项审阅staged文件，
  禁止提交真实LAN地址、凭据、私钥、未脱敏配置、Keil构建产物或巨大私有原始文件。
- [ ] 根据真实结果更新`docs/PLANS.md`、`docs/DECISION_LOG.md`、
  `docs/OPEN_QUESTIONS.md`、`docs/RESUME_TRACEABILITY.md`和`docs/milestones/M10.md`。
- [ ] 只有四个独立正式run全部真实PASS、Ubuntu复核PASS且恢复状态完整时，才能把M10改为
  `MET`。CPU/RSS和时延报告必须绑定确切build/profile；没有冻结阈值时不得写“低于目标”或
  “性能达标”，只能写实测分布和四场景门禁事实。
- [ ] 提交并push M10证据/文档，报告commit、push、每个场景结论和全部`NOT RUN/FAIL`。
  完成后立即停止，不开始任何后续阶段。

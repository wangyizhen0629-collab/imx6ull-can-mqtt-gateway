# 待确认问题

回答必须由命令输出、文件路径、照片/测量值或版本信息支持。解决某个问题不代表自动
授权下一 Milestone。

## i.MX6ULL 构建与访问

- 早期 PATH 查询未找到 ARM compiler 的结果保留在原 run；随后用户把已 relocate 的
  Buildroot SDK 放入仓库工作区 `ToolChain/arm-buildroot-linux-gnueabihf_sdk-buildroot/`。
  该 2.3 GiB 外部工具链已由 `.gitignore` 排除，不作为仓库源码提交。
- 板端只读证据 `artifacts/19700101T123711+0000-m2-board-audit/` 已确认真实 i.MX6ULL、
  ARMv7、Linux 4.9.88、Buildroot 2020.02-g65177d4；loader 名为
  `ld-linux-armhf.so.3`，libc symlink 指向 2.30。
- SDK 已确认 compiler prefix 为 `arm-buildroot-linux-gnueabihf`、GCC 7.5.0、glibc 2.30
  sysroot；最终 ELF 为 Cortex-A7/ARMv7 EABI5 hard-float，动态解释器与板端一致。完整
  证据在 `artifacts/20260829T131442+0800-m2-arm-cross-final/`。
- 板端 Buildroot 修订 `g65177d4` 与 SDK 修订 `gee85cab` 不同；M2 已用 SHA256 固定的
  `gatewayd` 完成真实动态加载和 loopback，因此该差异对当前 M2 binary 的运行兼容性
  已由实测关闭，不能外推到未来新增依赖或其他 binary。
- 当前已通过 Windows MobaXterm 访问目标，板端有 `scp`/`tar`；Ubuntu VM 与板端之间的
  直接传输路径是否可用，还是继续经 Windows 中转？
- M2 可使用 `/tmp` tmpfs 做临时部署（审计时可用 245 MiB）；该结论不适用于 M7 spool
  持久化目录。
- M6历史依赖审计确认Buildroot SDK原始sysroot不含libmosquitto开发文件；后续M6/M7已用
  私有ARMv7 libmosquitto 2.0.11在板端真实运行。M8 preflight又以相同库SHA256
  `b32c8ac4...f636`核对匹配头文件和动态符号，四个external-loop API及`want_write`
  5/5存在，因此版本/API问题已关闭；最终binary又加入`mosquitto_connect_async`并完成
  6/6链接复核。M8板端reactor行为已由最终真实门禁关闭。
- `can0` 已确认是 FlexCAN、clock 30 MHz；M2 controller loopback、timestamp 和错误
  DLC 路径已经批准并 PASS。测试后为 DOWN/STOPPED、loopback off，500000 bit timing
  仍在关闭状态保留。
- 板端 wall clock 未初始化并报告 1970-01-01；M2 artifact 使用原始 run_id 并另记主机
  接收时间。后续是由 NTP、RTC 还是测试前人工设置时间？

## M2 退出门禁结果

- ARM 交叉构建已由 `artifacts/20260829T131442+0800-m2-arm-cross-final/` PASS 关闭；首次
  feature 宏失败 run 保留在 `artifacts/20260829T131347+0800-m2-arm-cross/`。
- i.MX6ULL、MobaXterm 访问、临时 `/tmp`、`ip`/`candump`/`cansend` 已有只读证据；
  `artifacts/20260829T132938+0800-m2-board-deploy-verify/` 又确认 SHA256 固定的
  `gatewayd` 已安全传到 `/tmp/gatewayd-m2`。
- `artifacts/20260829T133148+0800-m2-board-loopback/` 保存该具体 binary 的动态加载、
  目标 ID、非目标 timeout、错误 DLC、`kernel_timestamp_ns`、`can_before/after` 和恢复
  状态，全部 PASS；执行前批准范围也已记录。
- `artifacts/20260829T134148+0800-m2-final-audit/` 对上传 tar 的 18 个原始文件逐字节
  复核并 PASS。第一次 UID/GID 比较方法失败的 run 原样保留。

因此 M2 退出门禁已满足。遗留项不是 M2 阻塞：板端时间源仍待后续确定；500000 bit
timing 在接口 DOWN 时保留。STM32 和物理 `candump` 已在 M3 中由项目所有者验收。

## STM32 Windows 工程

- 已解决：项目所有者确认实际 MCU 丝印为 STM32F103C8T6，板载晶振为 8 MHz；工程已
  放入 `stm32/firmware/imx6ull-can-mqtt-gateway/`。
- 已解决：实际方案采用 PA11/CAN_RX、PA12/CAN_TX 默认映射，不使用 PB8/PB9 remap，
  也不启用占用 PA11/PA12 的 USB 数据功能。
- 已解决：SYSCLK 72 MHz、PCLK1 36 MHz，CAN Prescaler 4、SJW 1 TQ、BS1 13 TQ、
  BS2 4 TQ，对应 500 kbit/s、约 77.78% sample point。
- 已解决：项目所有者确认 Keil Build 0 error/0 warning，三类确定性报文已在真实物理
  `candump` 中验证。按其决定不补建 M3-A～M3-D 的新 artifact。
- 非阻塞遗留：CubeMX 安装元数据与 executable 版本字符串不一致；探针固件版本和精确
  终端电阻读数未归档。这些信息不得在文档中填入推测值。

Ubuntu 不存在 `arm-none-eabi-gcc`/OpenOCD 已不再是待解决问题，也不阻塞 Linux 侧开发。

## M3-E 真实 `gatewayd` 接收

- 已解决：`gatewayd` 在真实 `can0` 完成两次1110帧短测，均 accepted=1110 且 timeout、
  reject、timestamp error、receive error 为0；干净复测期间 CAN 状态和错误计数无新增
  异常。
- 已决定：项目所有者取消原10分钟和按 ID counter-gap 门禁，接受 M3 完成；不再为此
  修改当前60000 ms超时上限。
- 保留边界：10分钟测试没有执行，不能在后续文档或简历中写成稳定性/可靠性结果。

## M4 自定义协议与静态解码

- 已解决：`protocol/vehicle.dbc` 已冻结三类消息的 Intel 小端位布局、缩放、偏移、单位、
  Rolling Counter 和 XOR 字段；协议明确是实验用自定义协议。
- 已解决：STM32 使用整数定点生成60秒语义车况并严格按 DBC 编码；当前42条向量、
  完整6660帧主机模型及60秒实物 `candump` 逐帧审计通过，CAN 错误计数增量为0。
  M4 已于2026-08-31关闭。
- 证据限制：Keil 0 error/0 warning和Download是项目所有者确认，完整原始控制台输出
  未归档；捕获没有固件镜像哈希，故只能证明行为与源码模型一致，不能证明密码学绑定。
- 已解决：Windows 上的语义测试 sanitizer 仍保留历史 `NOT RUN`，但当前 Ubuntu clone
  已在 M5 ASan+UBSan 全量12/12中补跑当前测试；LeakSanitizer 仍单独为 `NOT RUN`。
- 已解决：项目所有者已在真实 i.MX6ULL 物理 CAN 上运行 M5 ARM binary，新增解码器、
  有界队列和 mock sink 的基准/过载/信号退出证据已归档，M5 门禁关闭。原始日志没有
  `can_before/after`、正确 UTC 或 shell `wait` 精确退出码；这些限制禁止外推 CAN 错误
  增量、持续运行、性能或可靠性结论，但不再是 M5 功能门禁的待办。
- 非阻塞遗留：`-DCMAKE_BUILD_TYPE=Release` 候选 run 在既有 M1 `lifecycle.c`/`log.c`
  上触发 FORTIFY `-Werror`，已保留为 FAIL；默认 warning-clean 和 ARM 构建不受影响。
  是否单独安排历史模块的 Release/FORTIFY 清理，应由后续维护阶段决定，不能把本轮
  Release 描述为通过。

## 网络、Broker 和运行策略

- M6 host loopback已解决测试实例：使用临时Mosquitto/libmosquitto 2.0.11，Broker只监听
  `127.0.0.1:18884`、匿名、禁用持久化，并在测试后停止。该配置只服务M6主机功能证据，
  不是未来局域网/部署配置。
- M6局域网实例已确认为Windows Mosquitto 2.1.2，且已与板端私有libmosquitto 2.0.11
  完成真实1000-batch门禁；M6兼容性问题已关闭。M7也已使用独立device ID、topic、端口
  和受控启停完成跨主机门禁；真实局域网地址和凭据只保存在Git忽略的私密原始证据中。
- M7功能门禁已确认`/dev/root` ext4下的专用`/var/lib`目录适合本次进程崩溃恢复测试，
  `/tmp` tmpfs不作为耐久证据。生产容量、写入寿命、真实掉电语义和介质选型仍未知。
- M7原型已决定每条记录append后`fdatasync`，state按temp-sync-rename-directory-sync
  推进；不做静默drop。当前ENOSPC/sync失败采用fail-stop。真实介质是否承受该写放大、
  spool容量阈值和已确认前缀的安全回收/compaction策略仍待目标测试后决定。
- M7本次授权和角色已解决：Windows侧控制专用Broker/subscriber，板端记录一次核实PID的
  `kill -9`、wait、spool hash和重启日志，双方原始证据已对账。后续任何新run仍需重新
  取得相应外部状态修改授权。
- 目标板使用什么时间源，wall clock 是否可能跳变？

## 测试和部署授权

- 每次 `can0` 状态修改、STM32 烧录、Broker 启停、进程 kill、`/etc`/inittab 修改、
  reboot、压力测试和 24 小时测试由谁在执行前授权？
- 长时间运行 artifact 的备份和保留策略是什么？
- M9 使用 BusyBox `inittab` respawn，还是镜像中已有可验证的 supervisor？

## M1～M7 后续验证边界

- M1/M2 当前源码已随 M2 的 ARM `gatewayd` 使用真实 Buildroot SDK 完成交叉编译，并
  在 i.MX6ULL 上完成动态加载和 controller loopback；该结论仅覆盖当前 SHA256 binary。
- LeakSanitizer 在当前命令执行环境中因 `ptrace` 限制无法运行；M1 有关闭 leak 检测后
  的 ASan+UBSan 8/8 通过证据，M2 有同条件 9/9 通过证据，M4 有同条件11/11通过证据。
  若需要无泄漏结论，必须在不受该限制的环境另建 run；当前不得宣称已经通过 LSan。
- M5 已接入实际 producer/consumer、DBC 解码和 mock sink；主机定时合成111帧/s用例
  queue drop 为0，慢消费者过载和 `SIGTERM` 通过。项目所有者又在真实 i.MX6ULL 上
  使用同一 SHA256 ARM binary 完成物理基准和容量4慢消费者测试：基准3694条 queue
  drop 为0，过载3561条按策略 drop，两次均在 signal 15 后输出最终 summary。M5 已关闭。
- 后续若要形成可靠性或性能结论，仍需另行授权并新建 run，测试开始前启动 STM32，
  保存 `can_before/after`、正确时间源、精确进程退出码及预先规定的持续时间和采样方法；
  这些属于后续专项验证，不得倒填到本次 M5。
- M2 主机单测验证未绑定 CAN_RAW socket 的过滤选项和 datagram timestamp 解析；真实
  i.MX6ULL bind/controller loopback 已另有板端证据。两类结果必须分别引用，不能用
  controller loopback 外推物理 CAN 或性能。
- M6后续已用私有ARMv7 libmosquitto和真实i.MX6ULL物理CAN连接Windows Broker；正式
  subscriber 1000批/115335条连续记录，gateway/Broker 1033次publish/PUBACK对账一致。
  `artifacts/20260901T090837+0800-m6-lan-gate-review/`完成manifest 131/131和原始证据
  独立复核，M6门禁现为`MET`。正确UTC、性能、可靠性及停止边界1帧原因仍未解决。
- M7源码、主机普通/ASan+UBSan全量16/16和ARMv7交叉构建证据为
  `artifacts/20260901T093654+0800-m7-offline-final/`；跨主机退出证据为
  `artifacts/20260901T105414+0800-m7-lan-recovery-gate2/`。实际断线、一次`kill -9`、
  subscriber合并验证、ext4 spool及三类损坏恢复均通过，missing0、effective duplicate0，
  M7现为`MET`。项目所有者随后单独授权M8；最终Ubuntu/ARM和真实Windows Broker/
  i.MX6ULL门禁均通过，M8总门禁为`MET`。

## 本次已解决项

- STM32 开发环境：Windows STM32CubeMX + Keil MDK + ST-Link；Ubuntu 不负责 STM32 编译。
- STM32 定位：确定性 CAN 流量发生器/模拟 ECU，不承担网关复杂功能。
- STM32 引脚：PA11/CAN_RX、PA12/CAN_TX 默认 bxCAN 映射；不启用 USB 数据功能。
- M3-A～M3-D：项目所有者已按实际 Build、接线/终端检查和物理 `candump` 现象验收；
  原始日志未归档，不能写未提供的量化值。
- M3-E/M3：两次1110帧真实 `gatewayd` 短测正常；10分钟门禁由项目所有者豁免，M3
  已关闭，但没有10分钟、可靠性或性能结论。
- CAN 收发器：STM32 侧为外置 TJA1050；i.MX6ULL 板载 CAN 模块使用 TJA1042T/3。
- i.MX6ULL CAN 模块：已包含 CAN 控制器、TJA1042T/3、120 Ω、TVS 和 CANH/CANL 接口。
- 终端配置：STM32 侧 TJA1050 模块和 i.MX6ULL 板载 CAN 模块各有 120 Ω，设计上
  不再增加终端电阻。
- M1 配置基础：严格 schema、范围、重复/未知 key 拒绝、默认值 < 文件 < CLI 的
  优先级和配置日志凭据脱敏已有主机测试。
- M1 并发基础：mutex stats、固定记录布局、有界 ring buffer、producer timeout、
  close/drain/broadcast 和 self-pipe 信号生命周期已有主机单元测试。
- M2 板端身份/前置工具：i.MX6ULL ARMv7、Linux 4.9.88、Buildroot 2020.02、FlexCAN
  `can0`、`candump`/`cansend`、`scp`/`tar` 和临时 `/tmp` 空间已有只读证据。
- M2 ARM 构建条件：relocated SDK、compiler/sysroot、ARMv7 hard-float ABI 和
  warning-clean `gatewayd` 已有真实构建证据。
- M2 板端门禁：SHA256 固定 binary 的动态加载、controller loopback、目标/非目标 ID、
  DLC 拒绝、kernel timestamp 和恢复状态已有真实日志及最终审计，M2 已通过。
- M4 自定义协议：DBC、42条黄金向量、定点静态解码布局、完整6660帧语义场景和60秒
  实物逐帧审计已通过；M5 的当前 Ubuntu ASan+UBSan 全量12/12已覆盖当前语义测试，
  新增 binary 的板端实时解码已由后述 M5 证据补齐。
- M5：producer 执行 CAN receive/DBC decode/有界入队，consumer 调用 mock sink；
  warning-clean 与 ASan+UBSan 全量12/12、主机111帧/s零 queue drop、故意过载、
  `SIGTERM`、ARMv7 交叉构建，以及真实 i.MX6ULL 基准/过载/信号退出均已通过。
  板端证据为 `artifacts/20260831T132341+0800-m5-board-owner-final/`，M5 已关闭。
- M6主机基线：libmosquitto QoS 1单in-flight状态机、batch JSON、匹配PUBACK统计、
  低流量timer路径和subscriber seq验证已通过；最终loopback证据为
  `artifacts/20260831T135630+0800-m6-mqtt-final/`。这项只允许描述为Ubuntu x86_64
  loopback功能验证，不能写成i.MX6ULL或完整CAN--MQTT链路。
- M6目标门禁：真实i.MX6ULL物理CAN到Windows Broker的1000批及独立manifest/原始证据
  复核已通过，M6状态为`MET`。
- M7：固定格式spool、CRC、原子ACK cursor、尾部/state恢复、seq恢复、重连、单写者锁、
  独立stats和去重validator已通过主机/ARM与真实跨主机门禁；M7已关闭。保留问题是正确
  UTC、真实掉电、CAN物理错误原因、介质寿命、容量/compaction和长期可靠性，不属于本次
  功能门禁的已验证结论。
- M8：与M7相同SHA256的libmosquitto 2.0.11、最终ARMv7 binary、真实i.MX6ULL/Windows
  Broker重连、SIGKILL/state恢复、reactor计数和严格validator均已通过；门禁已关闭。

## M8 关闭后仍未解决的边界

- 板端wall clock仍为1970；正确UTC何时由产品部署阶段提供并单独验证？
- 真实掉电、存储掉电语义、性能/时延/吞吐、CPU/RSS、介质寿命、容量阈值、compaction
  和长期可靠性仍未运行。不得用M8短时功能门禁回答这些问题。
- M9的BusyBox init/respawn方案仍待项目所有者另行授权；M8关闭不自动开始M9。

“模块标称带终端”只解决硬件配置/采购问题；M3-C 已由项目所有者按检查现象简化验收，
但精确电阻读数没有归档，后续不得引用推测的欧姆值。

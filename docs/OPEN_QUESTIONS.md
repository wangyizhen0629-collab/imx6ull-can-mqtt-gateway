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
- 目标 rootfs/sysroot 是否包含 `libmosquitto.so`、`mosquitto.h`、pkg-config 元数据及
  M8 所需四个 external loop API？板端只发现 `libmosquitto.so(.1)`，没有 `/usr/include`
  或 pkg-config；精确版本和 SDK 开发文件仍未知。
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
- 首轮已完成：20条共享向量由 C 解码单测和独立 DBC 检查器共同验证；另对旧 STM32
  三类 base 的全部768种 counter 规律执行静态解码。默认主机和 ASan+UBSan 均11/11
  PASS，ARMv7 warning-clean 交叉构建 PASS。这些结果保留，但不能单独关闭 M4。
- 当前阻塞门禁：Windows 侧尚未按 DBC 从有物理意义的模拟车速、转速、油门等物理量
  编码报文，也没有对应的新 Keil Build、`candump` 和物理场景向量。完成这些输入后，
  Ubuntu 必须用新 artifact 复测 DBC/解码器；在此之前 M4 为 NOT MET，M5 不得开始。
- 非阻塞遗留：本轮没有目标板会话，没有部署新的 ARM binary，也没有修改 `can0`；因此
  “新增解码器在真实 i.MX6ULL 物理 CAN 上运行”为 `NOT RUN`。后续若在 M5 集成链路中
  验证，必须先获部署/接口或进程操作批准并使用新的 artifact。
- 非阻塞遗留：`-DCMAKE_BUILD_TYPE=Release` 候选 run 在既有 M1 `lifecycle.c`/`log.c`
  上触发 FORTIFY `-Werror`，已保留为 FAIL；默认 warning-clean 和 ARM 构建不受影响。
  是否单独安排历史模块的 Release/FORTIFY 清理，应由后续维护阶段决定，不能把本轮
  Release 描述为通过。

## 网络、Broker 和运行策略

- 测试专用 `device_id`、Broker hostname/address、端口、认证方式、topic 规则和 client ID
  冲突策略是什么？
- PC 测试实例使用哪个 Mosquitto 版本和控制方式？
- 目标板哪个目录/存储介质适合 spool durability 测试？容量和写入寿命限制是什么？
- 耐久策略选择每 batch sync 还是有上限的周期 sync？spool 满时如何处理？
- 目标板使用什么时间源，wall clock 是否可能跳变？

## 测试和部署授权

- 每次 `can0` 状态修改、STM32 烧录、Broker 启停、进程 kill、`/etc`/inittab 修改、
  reboot、压力测试和 24 小时测试由谁在执行前授权？
- 长时间运行 artifact 的备份和保留策略是什么？
- M9 使用 BusyBox `inittab` respawn，还是镜像中已有可验证的 supervisor？

## M1～M4 后续验证边界

- M1/M2 当前源码已随 M2 的 ARM `gatewayd` 使用真实 Buildroot SDK 完成交叉编译，并
  在 i.MX6ULL 上完成动态加载和 controller loopback；该结论仅覆盖当前 SHA256 binary。
- LeakSanitizer 在当前命令执行环境中因 `ptrace` 限制无法运行；M1 有关闭 leak 检测后
  的 ASan+UBSan 8/8 通过证据，M2 有同条件 9/9 通过证据，M4 有同条件11/11通过证据。
  若需要无泄漏结论，必须在不受该限制的环境另建 run；当前不得宣称已经通过 LSan。
- M5 接入实际生产者/消费者后，`queue_push_timeout_ms` 的默认值 50 ms 和“超时丢弃
  新记录”策略是否满足 111 帧/s 基准，需要板端证据决定；M1 并发功能测试不能替代。
- M2 主机单测验证未绑定 CAN_RAW socket 的过滤选项和 datagram timestamp 解析；真实
  i.MX6ULL bind/controller loopback 已另有板端证据。两类结果必须分别引用，不能用
  controller loopback 外推物理 CAN 或性能。

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
- M4 自定义协议：DBC、20条黄金向量、定点静态解码布局、checksum/error 清零语义和
  三类消息全部768种 counter 规律已有主机与 ASan+UBSan 证据；ARMv7 交叉构建已通过，
  但新增 binary 的板端运行仍为 `NOT RUN`。

“模块标称带终端”只解决硬件配置/采购问题；M3-C 已由项目所有者按检查现象简化验收，
但精确电阻读数没有归档，后续不得引用推测的欧姆值。

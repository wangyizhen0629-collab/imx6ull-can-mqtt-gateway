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
timing 在接口 DOWN 时保留；物理 CAN/STM32 属于 M3，未运行。

## STM32 Windows 工程

- 当前仓库尚无 STM32F103C8T6 `.ioc`/Keil 工程；M3-A 将新建还是导入哪个现有工程？
- Windows 上实际使用的 STM32CubeMX、Keil MDK、Device Pack 和 ST-Link 工具版本是什么？
- F103 板载晶振、Clock Tree、APB1 时钟和最终 500 kbit/s bit timing 参数是什么？
- PB8/PB9 是否可用，CAN Remap 是否在生成工程和真实引脚上都已验证？
- Keil Build 日志和 ST-Link 烧录记录采用什么可导出的证据格式？

Ubuntu 不存在 `arm-none-eabi-gcc`/OpenOCD 已不再是待解决问题，也不阻塞 Linux 侧开发。

## CAN 物理层

- STM32 侧 TJA1050 模块的实际供电、TXD/RXD 逻辑电平兼容性和引脚定义是否已按
  模块原理图核对？
- 接线完成且全部断电后，CANH--CANL 的实际测量值是多少？
- i.MX6ULL 板载 CANH/CANL/GND 接口的实际针脚/端子定义是否已与接线记录一致？

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

## M1/M2 后续验证边界

- M1/M2 当前源码已随 M2 的 ARM `gatewayd` 使用真实 Buildroot SDK 完成交叉编译，并
  在 i.MX6ULL 上完成动态加载和 controller loopback；该结论仅覆盖当前 SHA256 binary。
- LeakSanitizer 在当前命令执行环境中因 `ptrace` 限制无法运行；M1 有关闭 leak 检测后
  的 ASan+UBSan 8/8 通过证据，M2 有同条件 9/9 通过证据。若需要无泄漏结论，必须在
  不受该限制的环境另建 run；当前不得宣称已经通过 LSan。
- M5 接入实际生产者/消费者后，`queue_push_timeout_ms` 的默认值 50 ms 和“超时丢弃
  新记录”策略是否满足 111 帧/s 基准，需要板端证据决定；M1 并发功能测试不能替代。
- M2 主机单测验证未绑定 CAN_RAW socket 的过滤选项和 datagram timestamp 解析；真实
  i.MX6ULL bind/controller loopback 已另有板端证据。两类结果必须分别引用，不能用
  controller loopback 外推物理 CAN 或性能。

## 本次已解决项

- STM32 开发环境：Windows STM32CubeMX + Keil MDK + ST-Link；Ubuntu 不负责 STM32 编译。
- STM32 定位：确定性 CAN 流量发生器/模拟 ECU，不承担网关复杂功能。
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

“模块标称带终端”已经解决硬件配置/采购问题，但没有替代 M3-C 的真实电阻测量。

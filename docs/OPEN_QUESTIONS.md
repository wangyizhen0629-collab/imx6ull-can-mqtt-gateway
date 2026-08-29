# 待确认问题

回答必须由命令输出、文件路径、照片/测量值或版本信息支持。解决某个问题不代表自动
授权下一 Milestone。

## i.MX6ULL 构建与访问

- 精确的 Buildroot 2020.02 output tree 或外部 SDK 在哪里？
- 应用 ABI、交叉编译器 prefix/version、libc、float ABI、CPU flags 和 sysroot 路径是什么？
- 后续使用 SSH/SCP、NFS、串口还是其他方式访问目标？哪个非生产目录可安全部署测试？
- `tools/board/collect_board_info.sh` 在真实板端的只读输出是什么？
- 目标 rootfs/sysroot 是否包含 `libmosquitto.so`、`mosquitto.h`、pkg-config 元数据及
  M8 所需四个 external loop API？精确版本是什么？
- `can0` 的 bitrate clock、当前 error frame 能力和 Linux 4.9.88 驱动状态是什么？

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

## M1 后续验证边界

- M1 的 C11/pthread 代码尚未使用真实 Buildroot SDK 做 ARMv7 交叉编译，也未在
  i.MX6ULL 上运行；需要取得精确 SDK/sysroot 后在后续相应门禁中验证。
- LeakSanitizer 在当前命令执行环境中因 `ptrace` 限制无法运行；本轮仅有关闭 leak
  检测后的 ASan+UBSan 8/8 通过证据。若需要无泄漏结论，必须在不受该限制的环境另建
  run；当前不得宣称已经通过 LSan。
- M5 接入实际生产者/消费者后，`queue_push_timeout_ms` 的默认值 50 ms 和“超时丢弃
  新记录”策略是否满足 111 帧/s 基准，需要板端证据决定；M1 并发功能测试不能替代。

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

“模块标称带终端”已经解决硬件配置/采购问题，但没有替代 M3-C 的真实电阻测量。

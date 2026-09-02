# M10 Windows profile 准备尝试 2

结论：**Windows 准备门禁 PASS；M10 仍为 NOT MET**。

## 已通过

- Keil target 生成器一致性检查：PASS，退出码 0。
- M10 candump 分析器单元测试：PASS，7/7，退出码 0。
- `M10_111` 全量构建：ARMCC 5.06u6，0 error，0 warning。
- `M10_500` 全量构建：ARMCC 5.06u6，0 error，0 warning。
- `M10_1000` 全量构建：ARMCC 5.06u6，0 error，0 warning。
- 三套 `.axf/.hex` 均在构建完成后存在；仅记录大小和 SHA256，构建产品被 Git 忽略且不得提交。
- 工程 target 均保留调试信息，并使用原项目的 ARMCC 优化设置 `Optim=4`；各自显式定义 `ECU_TRAFFIC_PROFILE=111/500/1000`。

原始 `build_product_sha256.txt` 的 `MISSING` 是异步 uVision 进程竞态，已按 `race_correction.txt` 在不覆盖原证据的前提下追加复核。

## 仍未运行

- STM32 烧录与真实 CAN 速率验证：NOT RUN。
- 500 帧/s至少30分钟、1000帧/s至少30分钟：NOT RUN。
- 20轮 Broker 中断：NOT RUN。
- 111帧/s至少24小时：NOT RUN。
- Ubuntu `RelWithDebInfo` gatewayd 构建、测试、ARMv7交叉构建及 binary SHA 冻结：NOT RUN，等待 Windows 提交后由 Ubuntu 复核。

本报告只允许推进到 Ubuntu 复核停止点，不构成任何真实硬件性能或稳定性结论。

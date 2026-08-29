# M3 前置核验报告

## 结论

M2 退出门禁复核为 **PASS**，允许进入 M3。M3 已启动，但当前停在 M3-A，M3 总门禁
**NOT MET**。

当前 Ubuntu 虚拟机没有 STM32CubeMX 或 Keil UV4，仓库也没有 `.ioc`、`.uvprojx` 或
生成后的 STM32 工程。根据仓库双环境规则，不能用 Ubuntu 上的推测配置或源码审阅替代
Windows CubeMX 生成和 Keil Build。因此 M3-A 为
`NOT RUN - 需要用户在 Windows STM32CubeMX/Keil 中验证`。

M3-B～M3-E 均受前序门禁阻塞，未修改 STM32 源码，未连接或测量硬件，未烧录固件，
未修改 `can0`，也未执行 10 分钟测试。

## M2 前置证据复核

- ARM binary SHA256 与 M2 记录一致：
  `be27554bafac535e45908e881117a185965470f21ae9645f2fcb0ca0a1ba5595`。
- 板端原始 tar SHA256 与 M2 记录一致：
  `a538c92bc4aef201df3c8dd7069285d48c997af98ffd0378732b443086f54163`。
- ARM run 冻结的 26 个源码/构建定义文件 checksum 全部匹配。
- M2 最终审计仍记录 18/18 原始成员逐字节一致，目标/非目标/DLC/timestamp/CAN 恢复
  用例全部 PASS。

这些结果只确认 M2 可以作为 M3 的前置门禁，不构成任何 STM32、物理 CAN 或 M3 测试
结果。

## 后续所需条件

1. 在同一仓库的 Windows clone 中确认实际 STM32 板卡晶振和引脚可用性。
2. 用 STM32CubeMX 保存 `.ioc` 并生成 Keil 工程，记录工具版本、Clock Tree、APB1、
   PB8/PB9 Remap 和由真实时钟计算出的 500 kbit/s timing。
3. 在 Keil 中真实 Build M3-A；通过后才实施 M3-B，并再次真实 Build。
4. 将源码、工程和唯一 Windows artifact 提交/同步回仓库，再核验 M3-A/M3-B 门禁。
5. M3-C 必须由用户在全部断电的硬件上核对接线并实测 CANH--CANL；后续烧录、修改
   `can0` 和 10 分钟运行仍需按规则分别批准。

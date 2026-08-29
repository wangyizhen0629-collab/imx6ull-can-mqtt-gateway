# M2 最终 ARM 交叉构建报告

使用已执行 `relocate-sdk.sh` 的 Buildroot SDK 和仓库内 CMake toolchain file 完成配置与
warning-clean 构建。生成的 `gatewayd.armv7` 是 Cortex-A7/ARMv7 hard-float ELF32，动态
解释器为 `/lib/ld-linux-armhf.so.3`，依赖 `libpthread.so.0` 和 `libc.so.6`；这些与板端
只读审计的 ABI 线索一致。

二进制 SHA256 为
`be27554bafac535e45908e881117a185965470f21ae9645f2fcb0ca0a1ba5595`。

板端镜像报告 Buildroot `2020.02-g65177d4`，SDK 报告
`2020.02-gee85cab`。修订号不相同，因此离线检查不能代替这个二进制的真实板端运行。
本 run 没有部署/启动程序、修改 `can0` 或发送 CAN 报文；板端执行和 loopback 均为
`NOT RUN`，M2 门禁仍未满足。

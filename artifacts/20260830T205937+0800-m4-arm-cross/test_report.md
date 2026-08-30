# M4 ARMv7 交叉构建

使用已 relocate 的 Buildroot 2020.02-gee85cab SDK、GCC 7.5.0 warning-clean 构建全部
gateway 和测试目标。`gatewayd.armv7` 为 ARM ELF32、EABI5 hard-float，解释器
`/lib/ld-linux-armhf.so.3`，SHA256 为
`2f0c4680ddc1a4c39de4782515bae227e0d90ea47815e5e790c5966ba0425ab1`。

ARM 测试可执行文件没有在 x86_64 主机运行；`gatewayd.armv7` 没有部署到 i.MX6ULL，
因此本 run 不是目标运行或物理 CAN 解码证据。

# M5 ARMv7 交叉构建

结论：交叉构建范围 `PASS`。使用仓库外、已 relocate 且被 `.gitignore` 排除的 Buildroot
SDK（GCC 7.5.0）完成 warning-clean 构建。输出为 Cortex-A7/ARMv7、EABI5 hard-float、
ELF32 little-endian 动态可执行文件，解释器 `/lib/ld-linux-armhf.so.3`，SHA256 为
`567079d01f4fb1e682a959cd01bac3709e4062f42c1f18903596dc47181d0a01`。

本 run 没有把 binary 部署到 i.MX6ULL，也没有启动程序或修改 `can0`。目标部署、动态
加载、物理 CAN → 解码 → queue → mock sink 基准、慢消费者过载和板端 SIGTERM 全部
为 `NOT RUN`；需要可访问目标板及对部署、接口/进程操作的明确批准。

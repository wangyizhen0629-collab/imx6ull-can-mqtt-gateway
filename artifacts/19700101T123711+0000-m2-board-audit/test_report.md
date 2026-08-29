# M2 i.MX6ULL 只读审计报告

## 已确认

- 真实板端为 Freescale i.MX6 UltraLite、ARMv7，Linux 4.9.88，Buildroot
  2020.02-g65177d4。
- 动态加载器为 `/lib/ld-linux-armhf.so.3`，libc symlink 指向 glibc 2.30；这些信息有助于
  选择 SDK，但不能代替精确 toolchain/sysroot 验证。
- `can0` 使用 FlexCAN，clock 30 MHz，审计时为 DOWN/STOPPED，TX/RX/error 计数均为 0；
  `/proc/net/can` 没有接收 socket 条目。
- `/usr/bin/candump`、`/usr/bin/cansend`、`scp`、`tar` 可用；板端没有 cmake/gcc/cc。
- `/tmp` 为 tmpfs，审计时可用 245 MiB，适合 M2 临时部署，不代表持久化目录。

## 边界

板端时钟未初始化，报告为 1970-01-01；保留原 run_id，不伪造时间。本 run 只读，没有
修改 `can0`、进程、Broker、固件或配置，没有运行 ARM binary 或 loopback。M2 退出门禁
仍未满足。

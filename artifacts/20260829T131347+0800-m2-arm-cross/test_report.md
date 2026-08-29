# M2 首次 ARM 交叉构建

CMake 使用 GCC 7.5.0 的 `arm-buildroot-linux-gnueabihf` SDK 配置成功，pthread 探测成功。
编译在 `can_receiver.c` 失败：目标 glibc 2.30 的 `sys/socket.h` 在只启用
`_POSIX_C_SOURCE` 时没有公开 Linux 专用 `SO_TIMESTAMPNS`。

该 run 保持 FAIL。修复通过 CMake 同时声明 `_DEFAULT_SOURCE`，后续使用新 run 验证，
没有覆盖本日志。

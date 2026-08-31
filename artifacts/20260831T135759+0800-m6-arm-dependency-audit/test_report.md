# M6 ARMv7 依赖审计

ARMv7 M6 交叉构建：**NOT RUN**。

Buildroot GCC 7.5.0 toolchain 检测成功，但 CMake 在生成构建系统前按预期失败：SDK sysroot
没有 `mosquitto.h`、目标 `libmosquitto.so`/`.a` 或 `libmosquitto.pc`。因此没有编译、
生成、部署或运行 M6 ARM binary。历史板端审计只发现 rootfs 上有
`/usr/lib/libmosquitto.so(.1)`，不能代替版本匹配的开发头文件和可复现交叉链接输入。

所需条件：把与目标 rootfs 匹配的 libmosquitto 开发文件加入 Buildroot SDK/staging，
然后以新的唯一 run 重新配置和构建；不得让交叉编译误用 x86_64 主机库。

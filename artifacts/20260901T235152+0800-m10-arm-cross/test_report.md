# M10 ARMv7交叉构建报告

Buildroot GCC 7.5.0配置和warning-clean构建PASS。输出是ARM EABI5 hard-float ELF32，
解释器为`/lib/ld-linux-armhf.so.3`，NEEDED为`libpthread.so.0`、
`libmosquitto.so.1`、`libc.so.6`，无RPATH/RUNPATH。binary SHA256为
`7bb1d7299eac43d5a7a9b8f52981652c6ed3e3f3b29567ff74a1abc5f2b3edef`。

本run没有部署或在i.MX6ULL执行binary。M10采集器是BusyBox ash脚本，报告器在Ubuntu
运行，不被交叉链接进gatewayd；主机BusyBox回归不能替代目标BusyBox 1.31.1实测。


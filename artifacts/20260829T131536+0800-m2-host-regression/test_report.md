# M2 Linux feature 宏修正后的主机回归

为使目标 glibc 2.30 公开 Linux 专用 `SO_TIMESTAMPNS`，构建定义同时启用
`_POSIX_C_SOURCE=200809L` 和 `_DEFAULT_SOURCE`。该源码在 Ubuntu x86_64 上完成
warning-clean Debug 构建，CTest 9/9 PASS。

CTest 仅为创建未绑定的 PF_CAN socket 在受限沙箱外执行，没有 bind 或修改任何 CAN
接口。本 run 不是 ARM 或板端 loopback 证据。

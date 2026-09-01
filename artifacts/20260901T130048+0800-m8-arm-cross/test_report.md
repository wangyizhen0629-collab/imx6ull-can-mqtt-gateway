# M8 ARMv7 cross-build report

- 工具链：Buildroot GCC 7.5.0，目标 ARMv7 EABI5 hard-float，Linux 4.9 ABI。
- 目标开发集：与 M7 板端运行库 SHA256 相同的 libmosquitto 2.0.11。
- 首次配置：FAIL；只设置 `GATEWAY_MOSQUITTO_ROOT` 时，交叉查找规则拒绝目标 sysroot
  外的开发集，原始日志保留为 `configure.log`。
- 第二次配置/构建：PASS，但 ELF 审计发现构建机绝对 RPATH，不能用于最终门禁；原始
  `readelf-dynamic.log` 保留该失败。
- 最终重新配置：PASS，显式使用匹配的目标头文件/库并设置 `CMAKE_SKIP_RPATH=TRUE`。
- 最终 warning-clean 构建：PASS。
- 最终 ELF：32-bit little-endian ARM EABI5 hard-float，解释器
  `/lib/ld-linux-armhf.so.3`，依赖 `libmosquitto.so.1`，无 RPATH/RUNPATH。
- 最终 binary SHA256：
  `2c3841e6a18ea80a470bf7d2bb8deaed314fdd1a495dc8c2b5c9a4021a8a9a6b`。
- binary 对 `mosquitto_socket`、`mosquitto_loop_read`、`mosquitto_loop_write`、
  `mosquitto_loop_misc`、`mosquitto_want_write` 均有真实动态重定位引用：5/5 PASS。
- i.MX6ULL 部署/运行：NOT RUN——当前 Ubuntu 会话没有目标板交互通道，且修改目标
  进程/Broker 前需要项目所有者单独批准。

结论：ARMv7 交叉编译/API 链接门禁通过；不外推为板端运行结果。

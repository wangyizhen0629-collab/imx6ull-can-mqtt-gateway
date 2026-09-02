# M10 spool v2 ARMv7 RelWithDebInfo最终报告

- 工具链：Buildroot GCC 7.5.0
- 配置：`RelWithDebInfo`、`BUILD_TESTING=OFF`、`CMAKE_SKIP_RPATH=TRUE`
- 初次构建和clean verbose warning-clean rebuild：PASS
- 实际编译标志：`-O2 -g -DNDEBUG -Wall -Wextra -Wpedantic -Werror`
- ELF：32-bit LSB ARM EABI5 hard-float，动态解释器`/lib/ld-linux-armhf.so.3`
- NEEDED：`libpthread.so.0`、`libmosquitto.so.1`、`libc.so.6`
- RPATH/RUNPATH：无
- 重建前后SHA256：
  `07c185e6e7e862195982f37f41501407ca17fd25442ab7b00c224466a8f7be5e`

binary只位于Git忽略的
`build/20260902T121151-m10-spool-v2-arm-relwithdebinfo/gateway/gatewayd`，未复制进artifact、
未提交、未传输、未部署、未执行。交叉构建不证明板端运行或性能。

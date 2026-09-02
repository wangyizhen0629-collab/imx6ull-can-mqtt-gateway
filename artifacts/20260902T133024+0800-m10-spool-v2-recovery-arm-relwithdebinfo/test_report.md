# M10 spool v2恢复纠正 ARMv7 RelWithDebInfo报告

## 最终结果

- 纠正后的configure：PASS
- 初次build及clean verbose warning-clean rebuild：PASS
- 编译器：Buildroot GCC 7.5.0
- 编译参数包含：`-O2 -g -DNDEBUG -Wall -Wextra -Wpedantic -Werror`
- binary大小：312172 bytes
- SHA256：`b79c723a4561c936d8b9b8cf90e87ba6da79a30111746aae4c2d69fb7eff0e16`
- ELF：32-bit LSB ARM EABI5、hard-float、动态链接、带debug info、not stripped
- 解释器：`/lib/ld-linux-armhf.so.3`
- NEEDED：`libpthread.so.0`、`libmosquitto.so.1`、`libc.so.6`
- RPATH/RUNPATH：无
- clean rebuild前后大小与SHA256一致

binary只存在于Git忽略的`build/`目录，未提交、传输、部署或执行。

## 保留失败

第一次configure遗漏`IMX6ULL_SDK_ROOT`，按toolchain合同exit 1，且未生成binary；原始
`configure.log`保留。随后在独立`-v2` build目录显式使用仓库已有relocated SDK，最终
结果见`configure-v2.log`及其余日志。


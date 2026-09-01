# M9 ARMv7交叉构建

- 最终结果：`PASS_WITH_BOARD_NOT_RUN`
- 首次配置：`FAIL/exit 1`，SDK误用相对路径
- 第二次配置/构建：`PASS`，但审计发现构建机绝对RPATH，判定`FAIL`
- 最终配置/构建：启用`CMAKE_SKIP_RPATH=TRUE`，退出码`0/0`
- warning scan：`PASS`
- ELF：ARM EABI5 hard-float ELF32，解释器`/lib/ld-linux-armhf.so.3`
- 依赖：`libpthread.so.0`、`libmosquitto.so.1`、`libc.so.6`
- RPATH/RUNPATH：最终`ABSENT`
- binary SHA256：`6e8729417b3dc40c10a413459de5eca9be43ce58dfcc8a3b12e91f5c8d7ef958`
- 板端部署/执行：`NOT RUN`

失败尝试全部保留，不能把首次含RPATH的binary描述为可部署结果。

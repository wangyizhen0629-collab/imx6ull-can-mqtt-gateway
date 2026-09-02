# 范围与授权

- 目标：拉取Windows准备提交，复核M10 profile/分析器/Keil证据，运行Ubuntu测试，生成并
  验证新的ARMv7 `RelWithDebInfo` binary，更新M10记录后提交和push。
- 输入HEAD：`b25cab851c2daf8e7b19d6eb3338747d400d06c8`，与当时`origin/master`一致。
- Windows准备提交：`06eaf8efafe126f74330fc60dbd291b1dffe1cfe`。
- Windows交接提交：`b25cab851c2daf8e7b19d6eb3338747d400d06c8`。
- 允许的修改：M10分析器边界修复/回归、文档和本次唯一证据目录。
- 未修改：目标网络/CAN、`/etc`、init、进程、Broker、固件、依赖和已有证据。
- 既有四组未跟踪目录保持原样，未覆盖、删除或提交。
- 本run不授权烧录、部署、短硬件预演或长时间测试。

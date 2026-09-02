# 依赖与构建产物审计

- 本轮没有下载、安装或升级第三方依赖。
- Debug/ASan继续使用既有本机libmosquitto输入；ARM继续使用仓库已有且Git忽略的Buildroot
  relocated SDK和`build/m6-arm-private/stage`输入。
- ARM输入hash保持：`mosquitto.h`为
  `b977523c8e51cd5a0833b6f9601f74a952a909796170dd856057d92b7cc30d8b`，
  `libmosquitto.so.2.0.11`为
  `b32c8ac4defb2b2920fba2e42f263869508c42e3c1719440db37ffc8d8c2f636`。
- 新ARM binary仅位于Git忽略的build目录；artifact只保存文本检查输出，没有复制ELF。
- 候选提交不包含`build/`、`ToolChain/`、Keil `Objects/`/`Listings/`、目标文件、库、ELF、
  AXF、HEX、凭据或原始私有数据目录。


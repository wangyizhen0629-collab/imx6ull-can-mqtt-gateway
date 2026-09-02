# 依赖与构建产物审计

- 本轮没有下载、安装、升级或提交第三方依赖。
- host和sanitizer继续使用既有`/tmp/imx6ull-m6-mosquitto-root`的头文件/动态库输入。
- ARM继续使用既有Git忽略的Buildroot SDK与`build/m6-arm-private/stage`输入；其
  `mosquitto.h` SHA256为`b977523c8e51cd5a0833b6f9601f74a952a909796170dd856057d92b7cc30d8b`，
  `libmosquitto.so.2.0.11` SHA256为
  `b32c8ac4defb2b2920fba2e42f263869508c42e3c1719440db37ffc8d8c2f636`。
- `tracked-binary-audit-final.log`对候选变更路径返回无匹配；`build/`、`ToolChain/`、
  `Objects/`、`Listings/`、`private_raw`、目标文件、库、ELF、AXF和HEX均不在候选提交。
- 新ARM binary只在`.gitignore`覆盖的`build/`下，未复制到artifact或候选提交。

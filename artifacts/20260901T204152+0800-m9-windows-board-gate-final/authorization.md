# M9 Windows 真实板端续跑授权边界

- 仅继续 M9，禁止实现或测试 M10。
- 允许使用既有 Windows/OpenSSH/MobaXterm 路径访问指定 i.MX6ULL。
- 先对 `/tmp/m9-staging/gatewayd` 执行只读 size/SHA256/ELF/interpreter/NEEDED/RPATH 硬门禁；任一不符立即停止。
- 允许在备份后只修改 `/etc/inittab`、`/etc/init.d/gatewayd`、`/etc/default/gatewayd`、`/etc/gatewayd/gateway.conf`，并创建本轮 `/opt/gatewayd` 和 `/var/lib/gatewayd` 专用路径。
- 允许对已核验的 M9 supervisor/gatewayd 执行 reload、一次受控 restart、一次子进程 SIGKILL 和一次受控 reboot。
- storm 只用隔离 fake gateway 与临时目录，禁止用真实 gatewayd 制造重启风暴。
- 禁止修改 CAN、Broker、网络、STM32、固件、系统库或依赖包；禁止性能、压力、长时和 24 小时测试。
- 真实端点、用户名、凭据和私有配置仅保存于 Git 忽略的 `private_raw/`；公开证据必须脱敏并保存 raw/redacted SHA256 映射。

原始用户授权提示 SHA256：`7afc2680aea3a6079a8f2fc933b10d1b721bc3863bf0f25efb8d578ab1647951`。

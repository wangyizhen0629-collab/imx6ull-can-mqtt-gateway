# BusyBox 部署

部署文件安排在 M9。目标系统没有 systemd，最终使用 BusyBox 兼容启动脚本和经过验证的
respawn/supervisor。写入 `/etc`、修改 `inittab` 或启动服务前必须取得批准。

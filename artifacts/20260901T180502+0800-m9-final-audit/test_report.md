# M9首次最终审计

- 所有自动检查退出0：diff、BusyBox ash语法、inittab精确项、systemd禁止项、敏感信息和
  前六个M9 artifact manifest复核。
- 审计同时发现仓库工作树中的inittab/env示例权限为0664；内容与Git记录不受影响，但与
  部署文档建议的0644不完全一致。
- 随后只把这两个非可执行示例收紧到0644，并另建最终审计run；本run保持不变，不作为
  最终文件模式依据。

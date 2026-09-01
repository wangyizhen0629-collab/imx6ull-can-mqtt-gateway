# reboot recovery 脱敏 attempt 1

结果：**FAIL（未提交）**

PowerShell 原生命令异常格式化将真实 IPv4 缩写成点分数字后缀，首版只匹配完整四段
IPv4 的规则没有覆盖该残片。首版公开副本已按原字节和 SHA256 移入 Git 忽略的
`private_raw/quarantine/reboot_recovery_poll.attempt1_leaked.txt`；没有删除或覆盖。

v2 脱敏同时替换精确 endpoint、完整 IPv4 和 2～4 段点分数字残片，并在提交前用精确
endpoint、私网 IPv4、点分残片和凭据规则复扫。

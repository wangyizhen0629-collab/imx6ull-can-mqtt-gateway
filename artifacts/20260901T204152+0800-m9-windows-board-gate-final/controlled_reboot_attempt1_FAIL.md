# 受控 reboot 控制器 attempt 1

结果：**FAIL（本地轮询控制器错误；reboot 命令只发送了一次）**

`controlled_reboot.ps1` 已先取得 reboot 前 boot ID，随后执行一次且仅一次
`sync; /sbin/reboot`。目标断开后，第一次 SSH 重连探测向 stderr 写入预期的连接失败；
Windows PowerShell 因 `$ErrorActionPreference='Stop'` 将该原生命令 stderr 当作终止异常，
脚本在写入最终轮询证据前退出 1。

禁止重跑该脚本，以免发送第二次 reboot。后续只执行不含 reboot 命令的恢复轮询，并将
持久化 `pre_reboot.marker` 中的旧 boot ID 与重连后的 `/proc/.../boot_id` 比较；只有
boot ID 实际变化才判定真实 reboot 成功。

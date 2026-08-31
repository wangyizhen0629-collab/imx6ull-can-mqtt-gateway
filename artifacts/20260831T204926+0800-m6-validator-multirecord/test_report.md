# M6 validator 多记录回归第二次编排报告

- Run ID：`20260831T204926+0800-m6-validator-multirecord`
- 状态：**FAIL（测试编排错误）**
- Validator 三项测试：**NOT RUN**

PowerShell 函数参数误用了自动变量 `$input`，所以 `Start-Process` 在 Python 启动前拒绝
空参数。错误处理又将空进程对象转换为整数 `0`，导致 `*.exit.txt` 出现 `exit_code=0`；
这些值不是程序退出码，禁止解释成 PASS。

stdout/stderr 文件因进程未启动而不存在。本目录不覆盖、不删除也不复用；修正后的测试
必须使用新的唯一 run ID。本失败不产生 validator、Broker、板端或 M6 门禁结论。

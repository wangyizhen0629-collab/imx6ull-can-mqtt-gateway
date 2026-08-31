# M6 validator 多记录回归首次编排报告

- Run ID：`20260831T204653+0800-m6-validator-multirecord`
- 状态：**FAIL（测试编排错误）**
- Validator 三项测试：**NOT RUN**

PowerShell 测试函数误用自动变量 `$args`，导致 `Start-Process -ArgumentList` 收到空参数并
在 Python 启动前失败。`*.exit.txt` 中只有 `exit_code=`，不是实际退出码，禁止推测。

本目录不覆盖、不删除也不复用。修正后的测试必须使用新的唯一 run ID。该失败不说明
validator 源码 PASS 或 FAIL，也不产生 Broker、板端或 M6 门禁结论。

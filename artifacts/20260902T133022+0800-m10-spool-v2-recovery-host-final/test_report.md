# M10 spool v2恢复纠正 Debug报告

- Debug configure：PASS
- clean verbose warning-clean build：PASS（启用`-Wall -Wextra -Wpedantic -Werror`）
- 全量CTest：PASS，21/21
- M7：PASS，4/4
- M8：PASS，2/2
- M9：PASS，1/1
- M10：PASS，5/5

`test_spool`包含恢复data-before-state定向回归：子进程`_exit`留下state游标后的完整记录；
恢复sync故障时open失败且state逐字节不变；随后无故障reopen安全恢复。分别覆盖部分
segment和恰好填满segment。

以上均为Ubuntu主机离线测试，不是板端、CAN或真实Broker测试。


# M9 BusyBox supervisor 主机专项

- 结果：`PASS`
- 解释器：Ubuntu BusyBox 1.30.1 `ash`
- 命令：`busybox ash -x deploy/tests/test_gatewayd_supervisor.sh`
- 原始输出：`test.stdout.txt`
- 完整shell跟踪：`test.trace.txt`（239行）
- 退出码：`0`

实际执行并由脚本断言的路径：

1. fake gateway首次退出42，supervisor自动产生第二次start；
2. 受控restart将子PID从34替换为58；
3. stop后start次数保持3不变，start后增加到4；
4. 第二场景连续3次快速退出后出现`event=restart_storm`，1秒观察窗内没有第4次启动；
5. 2秒cooldown结束并把fake模式切为hold后出现第4次启动；
6. 两个supervisor均由测试脚本发送TERM并完成wait，临时目录由trap清理。

PID是本次主机临时进程号，只用于证明状态转换，不代表目标板PID。该run没有运行BusyBox
init/PID 1、没有修改`/etc/inittab`、没有重启目标板，也不能替代M9开机启动门禁。

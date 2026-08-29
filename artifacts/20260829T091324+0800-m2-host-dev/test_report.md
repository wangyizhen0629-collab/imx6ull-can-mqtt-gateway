# M2 主机开发 run

warning-clean 构建成功。CTest 8/9 通过，`test_can_receiver` 在创建未绑定 `PF_CAN` socket
时被执行沙箱拒绝，因此本 run 整体为 FAIL。失败发生在测试环境权限边界，不改写为
通过；后续沙箱外复核和最终测试必须使用新的 run。

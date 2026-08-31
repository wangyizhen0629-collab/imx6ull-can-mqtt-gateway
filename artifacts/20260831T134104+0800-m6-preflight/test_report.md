# M6 前置门禁复核

结论：**PASS，M5 门禁保持 MET，可以开始 M6**。

- 起点提交为 `ae4fd3fdff908eab5112a065e6e3cc23d88f8284`，开始创建本 run 前工作区干净；
- GCC 11.4.0 warning-clean 构建通过；
- 沙箱内 CTest 11/12，唯一失败是既有 M2 `PF_CAN` socket 被沙箱拒绝；
- 获准在沙箱外运行同一套测试后 12/12 PASS，其中 M5 `test_pipeline` PASS；
- 没有修改 `can0`、Broker、固件、`/etc` 或依赖。

该 run 只证明 M5 前置门禁，不是 M6 MQTT 功能证据。

# M8 host development test report

- 配置：PASS，GCC 11.4.0，Debug，warning-clean 选项由 CMake 目标启用。
- 构建：PASS。
- M8 CTest：2/2 PASS（`test_mqtt_sink`、`test_mqtt_reactor`）。
- 全量 CTest：16/17 PASS；唯一失败为 `test_can_receiver` 无法在受限沙箱创建
  `PF_CAN` socket。该错误与 M8 reactor 无关，原始输出保存在 `ctest-full.log`。
- 沙箱外全量复测：NOT RUN——执行审批未获用户在知情后的明确授权。
- Broker/external-loop 集成：本 run 未启动 Broker。

本 run 是开发阶段离线证据，不能证明真实 MQTT socket、重连、板端或硬件行为。

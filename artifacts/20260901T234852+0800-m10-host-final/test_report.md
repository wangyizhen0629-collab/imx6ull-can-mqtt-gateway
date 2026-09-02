# M10 Ubuntu主机测试报告

## 结果

- Debug配置：PASS。
- warning-clean构建：PASS，`build.log`无warning/error。
- 首次受限沙箱全量CTest：18/20；`test_can_receiver`无法创建PF_CAN descriptor，
  `test_mqtt_sink`无法保留TCP socket。CTest输出明确为FAIL；外围`tee`返回0不改变判定。
- 沙箱外全量复测v2：20/20 PASS。
- 最终工具源码M10专项v3：2/2 PASS。

M10回归实际覆盖BusyBox ash执行、PID文件、exe身份、缺失PID、拒绝覆盖、CPU/RSS计算、
四种场景合同，以及错误环境、短时长、断网轮数不足和queue drop拒绝。`baseline_24h`
测试使用合成86401行CSV验证门禁计算，但没有等待24小时，也不构成稳定性证据。

## 判定

M10离线工具和回归为PASS。真实硬件场景均未执行，不能据此关闭M10总门禁。


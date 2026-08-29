# M2 最终主机测试报告

- deadline 边界复核后增加“总 deadline 到达立即 timeout 退出”修正。
- GCC 11.4.0 Debug warning-clean 构建：PASS。
- CTest：9/9 PASS；M2 `test_can_receiver` 验证未绑定 CAN_RAW filter 选项、主机内核
  timestamp、ID/DLC/短长 datagram/缺 timestamp/timeout 错误路径；既有 M1 测试通过。
- 受限沙箱禁止 PF_CAN socket，因此 CTest 在沙箱外执行；没有 bind、读取或修改 `can0`。

ARMv7 交叉编译和 i.MX6ULL controller loopback 均为 `NOT RUN`，故本 run 不能使 M2
退出门禁通过。

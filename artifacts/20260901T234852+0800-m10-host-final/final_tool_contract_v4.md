# M10最终工具合同v4补充

先前文件和manifest保持不变。本补充绑定`source_sha256.v2.txt`和`ctest-m10-v4.log`。

最终v4在既有真实环境、时长、每秒采样和零错误合同上增加：

- CAN必须分别提供`0x100`、`0x101`、`0x102`帧数及counter gap；
- 三个ID帧数之和必须等于`frames_total`；
- `frames_total >= target_rate_fps * duration_seconds`；
- CAN error frame、RX error/drop/overrun增量必须分别为0；
- 最终drain后的MQTT unique record必须等于CAN总帧数。

直接回归为采集/报告3/3、场景validator 7/7；最终CTest M10标签2/2 PASS。新增失败用例
确认不足帧数和按ID计数不守恒均被拒绝。合成数据只验证合同，不构成硬件性能结果。


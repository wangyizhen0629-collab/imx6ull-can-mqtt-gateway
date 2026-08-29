# M2 最终主机测试报告

## 真实结果

- GCC 11.4.0 Debug warning-clean 构建：PASS。
- CTest：9/9 PASS；其中 5 个既有 M1 单元测试、3 个 CLI smoke test 和 1 个 M2
  `test_can_receiver`。
- M2 单测在未绑定 `CAN_RAW` socket 上实际设置并读回三条精确过滤器和
  `SO_TIMESTAMPNS`；datagram socket 用例实际取得内核纳秒辅助时间戳。
- 注入用例覆盖标准目标帧、非目标 ID、扩展帧、RTR、DLC 7、短/长 datagram、缺失
  timestamp 和 timeout。上述都是 x86_64 主机功能测试，不是性能结果。

受限沙箱禁止创建 `PF_CAN` socket，故最终 CTest 在沙箱外执行；该测试只创建未绑定
socket 并读写 socket 选项，没有 bind、读取或修改任何 `can0`。

## NOT RUN

- ARMv7 交叉编译：缺少已验证 Buildroot SDK/sysroot 与 compiler/ABI 信息。
- i.MX6ULL 部署和 controller loopback：目标板不可用，也没有执行接口状态修改。
- `0x100`/`0x101`/`0x102` 板端接收、非目标 ID 过滤和真实 CAN socket timestamp：
  需要 ARM 二进制、目标板、`can-utils` 和执行前批准。

因此本 run 只通过 M2 主机子集，不能使 M2 退出门禁通过。

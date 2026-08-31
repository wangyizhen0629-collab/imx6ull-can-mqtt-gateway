# M6 Ubuntu 主机最终回归

结论：**PASS（x86_64 主机构建和单元回归）**。

- 使用 GCC 11.4.0、C11、`-Wall -Wextra -Wpedantic -Werror`；
- 使用实际 libmosquitto 2.0.11 开发文件和动态库；
- fresh configure/build PASS；
- 沙箱内 12/13，唯一失败为既有 PF_CAN 权限限制；
- 沙箱外相同 CTest 13/13 PASS，M6 `test_mqtt_sink` PASS；
- 新增测试覆盖 batch JSON 编码、容量不足、非单调 seq、实际库加载、配置边界，以及
  ring buffer timed pop；既有 M1～M5 回归保持通过。

该 run 不启动 Broker，不证明1000-batch、ARMv7或目标板行为。

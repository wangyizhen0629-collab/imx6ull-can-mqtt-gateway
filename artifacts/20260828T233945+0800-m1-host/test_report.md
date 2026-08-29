# M1 主机构建失败记录

- 结果：**FAIL**
- CMake 配置：PASS
- warning-clean 主机构建：FAIL
- CTest：**NOT RUN** —— 编译未通过，未生成完整测试集合
- 原因：`gateway/tests/test_config.c` 使用 `mkstemp()` 时遗漏 `<stdlib.h>`，GCC 在
  `-Werror=implicit-function-declaration` 下停止构建。
- 处置：保留本 run 不变；修复后使用新的 run_id 重新执行完整门禁。
- 硬件/板端/CAN/MQTT：**NOT RUN** —— 不属于 M1，且未连接或修改任何设备状态。

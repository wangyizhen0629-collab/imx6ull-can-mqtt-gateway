# M4 语义车况主机测试

结论：`PASS`。当前 Windows clone 使用 MinGW GCC 6.3.0，以 C11、`-Wall`
`-Wextra -Wpedantic -Werror` 编译 `test_vehicle_decoder.c` 与生产解码器，编译器
无输出且退出码为 0。测试随后以退出码 0 完成：

- 42 条共享黄金向量，其中 31 条取自实物捕获、8 条静态边界、3 条错误路径；
- 完整 60 秒确定性车况模型：6000 帧 `0x100`、600 帧 `0x101`、60 帧
  `0x102`，合计 6660 帧；
- DBC 独立检查：3 条消息、42 条向量全部通过。

第一次编译曾因 PowerShell/旧 MinGW 的路径宏引号及 `strtok_r` 可见性失败；修正的是
测试基础设施，失败未隐藏，详见 `strict_compile.log`。此 run 不是 Ubuntu CMake/CTest、
ARM 交叉构建、STM32 Build 或实物 CAN 证据。

# M4 最终主机测试

默认 CMake 配置完成 GCC 11.4.0 warning-clean build，全量 CTest 11/11 PASS，其中 M4
2/2 PASS。M4 覆盖20条共享黄金向量、独立 DBC 位/缩放检查及 STM32 三类消息全部
768种 counter 规律。

CTest 仅为既有 M2 测试创建未绑定 `PF_CAN` socket 在受限沙箱外执行；没有 bind、修改
`can0` 或启动板端进程。本 run 是 x86_64 主机静态功能证据，不是板端实时解码证据。

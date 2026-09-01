# M7 Broker 测试前的实际结果

以下均为真实执行结果，不作为最终 run 替代品：

- Debug 主机构建：PASS，GCC 11.4.0，`-Wall -Wextra -Wpedantic -Werror`。
- 受限沙箱首次全量 CTest：15/16 PASS；既有 M2 `test_can_receiver` 因无法创建
  `PF_CAN` socket 失败。该失败保留，不改写成 PASS。
- 沙箱外只读复跑全量 CTest：16/16 PASS；M7 标签 2/2 PASS。
- ASan+UBSan M7 标签：2/2 PASS；`detect_leaks=0`，故 LeakSanitizer 为
  `NOT RUN`。
- ARMv7 交叉构建：第一次仅传 `GATEWAY_MOSQUITTO_ROOT`，CMake 因交叉查找根规则找
  不到私有库而 FAIL；随后显式传目标头文件和目标库后构建 PASS。
- 最终 ARMv7 静态审计构建：GCC 7.5.0 warning-clean PASS，输出为 ARM EABI5
  hard-float ELF32，解释器 `/lib/ld-linux-armhf.so.3`，依赖
  `libpthread.so.0`、`libmosquitto.so.1`、`libc.so.6`，无 RPATH/RUNPATH。
- 临时 Broker/subscriber、Broker 断开/恢复、SIGKILL：`NOT RUN`，等待按仓库规则
  单独批准。
- i.MX6ULL/物理 CAN/真实存储介质：`NOT RUN`，本轮没有目标板访问与状态修改批准。

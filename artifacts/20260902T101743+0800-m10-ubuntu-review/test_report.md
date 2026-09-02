# M10 Ubuntu复核报告

## 结论

Ubuntu停止点复核为**PASS**，正式ARMv7 `RelWithDebInfo`输入已生成并冻结SHA；M10真实
硬件及长时间退出门禁仍为**NOT MET**。没有执行或推测任何硬件结果。

## 仓库与Windows证据

- Ubuntu从`f67b3652e382dc41d97750b74df71b5d6d1d88cb`快进到交接HEAD
  `b25cab851c2daf8e7b19d6eb3338747d400d06c8`；拉取前的四组既有未跟踪目录未动。
- 复核Windows准备提交`06eaf8efafe126f74330fc60dbd291b1dffe1cfe`和交接提交
  `b25cab851c2daf8e7b19d6eb3338747d400d06c8`。
- 四个Windows artifact的原始manifest含CRLF。Ubuntu直接`sha256sum -c`把每个文件名
  末尾的`\r`当作文件名一部分，四次均exit 1；原始失败见
  `windows_manifest_direct_attempt.log`。只读执行
  `tr -d '\r' < artifact_manifest.sha256 | sha256sum -c -`后共68/68 PASS，见
  `windows_manifest_checks.log`。没有改写Windows证据。
- 三个Keil ARMCC 5.06u6 target的既有原始rebuild日志仍分别为0 error/0 warning；本次
  Ubuntu没有重新运行Keil或烧录。

## 代码复核与回归

复核发现`candump`文本扩展帧使用8位ID（例如`00000100`），并不会把`CAN_EFF_FLAG`直接
打印进数值。原分析器会把这个扩展帧错误当作标准`0x100`接受。本run保留文本ID宽度，
在没有ERR/RTR/EFF数值标志时将超过3位的ID标记为EFF，并新增拒绝该边界的回归。

- 当前源码分析器：8/8 PASS，见`analyzer_unit_tests_v3.log`。
- Keil target生成器一致性：PASS，见`keil_target_generator_check.log`。
- 已归档M4真实抓包重放：6660帧，6000/600/60，持续59.988758秒，counter gap、error frame
  和CAN错误增量均为0，PASS，见`m4_replay_summary_v2.json`。
- Ubuntu Debug warning-clean构建：PASS。
- 受限沙箱CTest：19/21；`test_can_receiver`缺PF_CAN权限，`test_mqtt_sink`不能创建保留
  socket，真实失败保存在`ctest_sandboxed.log`。
- 沙箱外最终当前源码CTest：21/21 PASS，M10标签3/3 PASS，见`ctest_unsandboxed_v3.log`。

另执行Ubuntu主机`RelWithDebInfo`诊断构建，因glibc FORTIFY在`lifecycle.c`的`write`
返回值和`log.c`时间戳`snprintf`上产生既有warning，且项目启用`-Werror`，所以为FAIL；
见`host_build.log`。这不是ARM正式输入的结果，也没有被改写为PASS。

## ARMv7 RelWithDebInfo

- 配置/构建/clean verbose rebuild：PASS；实际编译参数包含
  `-O2 -g -DNDEBUG -Wall -Wextra -Wpedantic -Werror`。
- SHA256：`d234f2c5f0cc732fd56bc43cc2b8f59491944111b430409ca0ab5b6bb07e4fbf`；普通构建与
  clean verbose rebuild前后一致。
- `file`：ELF32、ARM、EABI5、hard-float、动态链接、含debug info、未strip。
- interpreter：`/lib/ld-linux-armhf.so.3`。
- NEEDED：`libpthread.so.0`、`libmosquitto.so.1`、`libc.so.6`。
- RPATH/RUNPATH：无。
- 目标`libmosquitto.so.2.0.11`输入SHA仍为
  `b32c8ac4defb2b2920fba2e42f263869508c42e3c1719440db37ffc8d8c2f636`。

binary位于Git忽略的`build/m10-ubuntu-review-arm-relwithdebinfo/gateway/gatewayd`，未提交、
未私有传输、未部署、未在i.MX6ULL运行。因此只支持构建输入冻结，不支持性能结论。

## NOT RUN与停止点

STM32烧录、三档短candump、120秒板端指标预演、短端到端对账、500/1000帧/s各30分钟、
20轮Broker中断和111帧/s 24小时全部`NOT RUN`，详见`not_run.md`。本run没有修改目标
网络/CAN、`/etc`、init、进程、Broker、固件或依赖。

下一步只能由Windows先拉取包含本修复和本报告的最终提交，再按硬件清单逐项申请精确
授权；Ubuntu本轮在M10停止，不进入任何后续Milestone。

# M8 持续输入重连修复 Ubuntu 验证报告

## 范围与源码身份

- 本次只验证 M8 提交 `6c2ed510f75fe8dc762e2ac3586c7cc5a750645e`，没有修改生产
  源码、测试源码或 M9 内容。
- `git merge-base --is-ancestor 6c2ed510f75fe8dc762e2ac3586c7cc5a750645e HEAD`
  退出 0；测试开始时 HEAD 正是该提交。
- fresh build 目录检查退出 0；普通、sanitizer 和 ARM 三个目录均由本次 configure
  新建。普通构建日志明确重新编译了 `mqtt_sink.c` 和 `test_mqtt_sink.c`。
- 当前测试源码包含 `test_durable_reconnect_during_continuous_consume()`；该测试不显式
  调用 poll，使用绑定但不 listen 的临时 loopback 端口，并断言持续 consume 期间 MQTT
  connect attempts 大于 1。

## 结果汇总

| 项目 | 结果 | 真实证据与限制 |
| --- | --- | --- |
| Ubuntu warning-clean build | **PASS** | fresh Debug/`BUILD_TESTING=ON` configure、build退出0；三份build日志合并审计`warning_count=0` |
| `test_mqtt_sink` | **PASS** | 沙箱外直接运行本次新binary退出0；受限沙箱首次因禁止创建AF_INET socket退出1，失败日志保留 |
| M8 CTest | **PASS** | 沙箱外`test_mqtt_sink`和`test_mqtt_reactor`为2/2 PASS；受限沙箱首次为1/2，原因同上 |
| 全量 CTest | **PASS** | 沙箱外17/17 PASS；受限沙箱15/17，只有PF_CAN和AF_INET socket权限失败，两份日志均保留 |
| ASan | **PASS** | fresh sanitizer build；两个直接测试及M8 CTest 2/2退出0，无ASan报告 |
| UBSan | **PASS** | 同上，`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`，无UBSan报告 |
| LeakSanitizer | **NOT RUN** | 使用`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`；没有执行泄漏检测，不能写成无泄漏 |
| ARMv7 build | **PASS** | fresh Debug/`BUILD_TESTING=OFF`/warning-clean Buildroot GCC 7.5.0构建退出0 |
| ELF/API/RPATH 审计 | **PASS** | ELF32 little-endian ARM、EABI5 hard-float、interpreter正确、NEEDED含`libmosquitto.so.1`、RPATH/RUNPATH计数0、external-loop API 5/5动态重定位 |
| i.MX6ULL/Windows Broker/CAN | **NOT RUN** | 本Ubuntu任务明令禁止；需要项目所有者在Windows/目标板用本次新binary另建run执行 |

## 主机构建与测试细节

- Ubuntu 22.04.5、kernel 6.8.0-138、GCC 11.4.0、CMake 3.22.1、GNU Make 4.3。
- 主机libmosquitto开发文件来自已有的临时解包root；系统`pkg-config`和`dpkg-query`
  没有注册该开发包，相关失败输出已保留。实际头文件宏和库文件名均确认版本2.0.11，
  没有安装或替换依赖。
- 当前 `mqtt_sink.c` SHA256：
  `256833c498082c7cb863f361cece1532b78c5f87ea7c16ff1e4f9607b019b112`。
- 当前 `test_mqtt_sink.c` SHA256：
  `98ff9d60170d2a67301bb8ee07c1f8879fde590f8975e7b4316c14e44dc868b3`。
- 新普通 `test_mqtt_sink` SHA256：
  `fb9fcb502f856b881efc06efa3a750dc48aa81a83c9557677d94713cf857cecb`。

受限沙箱禁止测试所需socket，所以首次直接测试和CTest失败。未覆盖这些日志；同一fresh
binary随后在沙箱外运行通过。最终PASS只表示Ubuntu主机功能回归，不替代真实Broker或
i.MX6ULL行为。

提交审计中，暂存前`git diff --check`退出0；暂存后全量检查因`script`保留的PTY CRLF
把原始终端行报告为trailing whitespace并退出2。没有为通过格式检查而改写真实日志；
只检查人工维护Markdown的cached check退出0。该格式限制不改变命令退出码或测试结论。

## ARMv7交付身份

- 使用既有SDK：Buildroot 2020.02，GCC 7.5.0；没有安装、替换或重建依赖。
- 目标libmosquitto 2.0.11 SHA256：
  `b32c8ac4defb2b2920fba2e42f263869508c42e3c1719440db37ffc8d8c2f636`。
- 新binary绝对路径：
  `/home/wangyizhen/projects/imx6ull-can-mqtt-gateway/build/m8-continuous-reconnect-fix-arm/gateway/gatewayd`
- 大小：`198180` bytes。
- 新binary SHA256：
  `c27cae52c6e746035e058cc77e78cb0dc1003f58f3036312b000579e0af9e368`。
- 修复前binary SHA256
  `2c3841e6a18ea80a470bf7d2bb8deaed314fdd1a495dc8c2b5c9a4021a8a9a6b`
  只作历史对照；本次新binary未强行匹配旧SHA。
- 第一次external-loop唯一符号计数脚本因awk转义错误退出非零，失败日志保留；重试文件
  实际列出5个不同符号并计数为5，没有重建或替换binary。

该binary是被`.gitignore`排除的构建产物，**不得提交Git**。项目所有者需通过私有方式
复制到Windows路径
`artifacts/20260901T143500+0800-m8-windows-reactor-gate/private_raw/incoming/gatewayd.fixed`
并在部署前重新计算SHA256。

## 门禁结论

本提交修复的Ubuntu warning-clean、持续consume重连单测、M8专项sanitizer以及ARMv7
构建/ELF门禁均通过。真实Windows Broker恢复和i.MX6ULL运行尚未用新binary重跑，所以
M8总门禁仍为 **NOT MET**，不得进入M9。

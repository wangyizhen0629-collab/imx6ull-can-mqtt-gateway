# M8 静默 listener 异步重连修复 Ubuntu 验证报告

## 范围与提交身份

- 测试开始时 HEAD 为
  `25681d871a32ed3936962144418058d1af2700b4`，且对应ancestor检查退出0。
- 本次只验证该M8修复，没有修改生产源码、测试源码或M9内容。
- artifact和三个build目录均为本次新建；fresh目录检查退出0，构建日志显示当前
  `mqtt_sink.c`及`test_mqtt_sink.c`被重新编译。
- 当前源码SHA256：`mqtt_sink.c`为
  `0497917a2e8b2ed78baffc6d8c12010ae4e70dbb3e34dedab4e18e561b422fb5`，
  `test_mqtt_sink.c`为
  `2416a5a38d9f5c1cbd3cfc8373568ec2b9127983cad99b324446e382fb3f3e4a`。

## 焦点测试语义

`test_durable_reconnect_during_continuous_consume()`的当前源码和自动审计证明：

1. `create_silent_loopback_listener()`创建`AF_INET/SOCK_STREAM` socket，在loopback随机
   端口执行`bind`和`listen`；测试文件中`accept/send/sendto/write`调用计数为0，因此
   内核完成TCP握手后测试端不会读取MQTT CONNECT或返回CONNACK。
2. 测试函数内`gateway_mqtt_sink_poll`显式调用计数为0；只调用四次
   `gateway_mqtt_sink_consume()`模拟consumer持续有输入。
3. 四次consume分别由`CLOCK_MONOTONIC`包围，并逐次断言耗时严格小于250 ms。
4. 末尾断言`spool_pending_records == 4`。
5. 末尾断言`MQTT_CONNECT_ATTEMPTS > 1`。

普通fresh binary直接执行退出0；随后`ctest --repeat until-fail:10`连续10次全部PASS。
因此仅该重复run就执行了40次consume，40个逐次`<250 ms`断言以及10组pending/attempts
断言均通过。测试当前不打印各次耗时数值，所以不能报告未采集的具体毫秒值。

## 结果汇总

| 项目 | 结果 | 证据与边界 |
| --- | --- | --- |
| Ubuntu warning-clean build | **PASS** | fresh Debug/`BUILD_TESTING=ON` configure和build退出0，三份build合并`warning_count=0` |
| 焦点`test_mqtt_sink`直接运行 | **PASS** | 本次fresh binary退出0 |
| 焦点测试重复 | **PASS** | `until-fail:10`连续10次通过 |
| M8 CTest | **PASS** | `test_mqtt_sink`、`test_mqtt_reactor`为2/2 PASS |
| 全量 CTest | **PASS** | 沙箱外17/17 PASS |
| ASan | **PASS** | fresh sanitizer直接焦点测试及M8 CTest 2/2通过，无ASan报告 |
| UBSan | **PASS** | `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`，无UBSan报告 |
| LeakSanitizer | **NOT RUN** | `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`，未执行泄漏检测 |
| ARMv7 build | **PASS** | fresh Debug/`BUILD_TESTING=OFF`，Buildroot GCC 7.5.0 warning-clean |
| ELF/API/RPATH | **PASS** | ELF32 little-endian ARM、EABI5 hard-float、解释器正确、NEEDED含`libmosquitto.so.1`、RPATH/RUNPATH为0 |
| i.MX6ULL/Windows Broker/CAN | **NOT RUN** | 本Ubuntu任务未获授权也未执行；需要用本次新binary另建真实门禁run |

## ARM binary交接

- 目标libmosquitto 2.0.11 SHA256：
  `b32c8ac4defb2b2920fba2e42f263869508c42e3c1719440db37ffc8d8c2f636`。
- binary绝对路径：
  `/home/wangyizhen/projects/imx6ull-can-mqtt-gateway/build/20260901T160813-m8-silent-listener-arm/gateway/gatewayd`
- 大小：`198576` bytes。
- SHA256：`2e3976727d57f850223ec3b0b3713c930d96f75375897f7c1fe69dcfc2e1548b`。
- 动态重定位包含原有5个external-loop API及新增`mosquitto_connect_async`，唯一符号
  计数为6。

该ARM binary是被Git忽略的构建产物，**未提交且不得提交Git**。需要目标板验证时由项目
所有者通过私有方式复制，并在部署前重新核对大小和SHA256。

## 门禁结论

目标提交的Ubuntu焦点功能、普通全量回归、M8 sanitizer和ARM交叉构建门禁均通过。
真实Windows Broker/i.MX6ULL恢复尚未用该binary执行，因此M8总门禁仍为 **NOT MET**；
不得进入M9。

## 提交边界审计

- 本次提交仅包含该唯一artifact目录；四个开始前已存在的无关未跟踪目录保持未暂存、
  未修改。
- 禁止产物扫描未发现`gatewayd`或其他build目录内容，敏感信息模式扫描计数为0；IPv4
  扫描仅命中焦点测试证据中的`127.0.0.1` loopback。
- 原始终端证据由`script`生成并原样保留CRLF，因此全量
  `git diff --cached --check`真实退出2并报告这些日志的行尾空白；人工维护的Markdown
  专项检查退出0。未为追求格式检查通过而改写原始证据。

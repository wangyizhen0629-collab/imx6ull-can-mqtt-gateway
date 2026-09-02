# M10 sanitizer最终工具合同v4补充

先前文件和manifest保持不变。本补充绑定`source_sha256.v2.txt`和`ctest-m10-v4.log`。
加入按CAN ID计数、速率乘时长下限、CAN/MQTT总数对齐和读取错误强化后，M10标签在
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`及UBSan halt/stacktrace下仍为2/2 PASS，
无ASan/UBSan诊断。LeakSanitizer继续为`NOT RUN`。


# M10 spool v2恢复纠正 ASan+UBSan报告

- Debug sanitizer configure：PASS
- clean build：PASS
- 全量CTest：PASS，21/21
- ASan/UBSan诊断：未见
- LeakSanitizer：NOT RUN（`detect_leaks=0`）

本结果仅验证Ubuntu主机离线路径，不替代板端或真实掉电测试。


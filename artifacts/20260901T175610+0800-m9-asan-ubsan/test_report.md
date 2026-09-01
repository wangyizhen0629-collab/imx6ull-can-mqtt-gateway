# M9 ASan+UBSan回归

- 结果：`PASS`
- 配置/构建/CTest退出码：`0/0/0`
- 全量CTest：`18/18`，M9专项`1/1`
- ASan：`detect_leaks=0:halt_on_error=1`
- UBSan：`halt_on_error=1:print_stacktrace=1`
- LeakSanitizer：`NOT RUN`，原因是当前执行环境由ptrace管理

该run不代替目标板BusyBox/init门禁。

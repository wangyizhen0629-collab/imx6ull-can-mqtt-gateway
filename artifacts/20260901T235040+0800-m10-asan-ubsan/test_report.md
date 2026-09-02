# M10 ASan+UBSan报告

Debug sanitizer构建成功。使用`detect_leaks=0:halt_on_error=1`和UBSan halt/stacktrace在
沙箱外执行全量CTest v2，20/20 PASS；最终M10工具源码专项v3为2/2 PASS。日志中没有
ASan或UBSan诊断。

LeakSanitizer因当前执行环境由ptrace管理而明确为`NOT RUN`，没有写成PASS。本run不含
真实板端、压力、断网、24小时或性能测试。


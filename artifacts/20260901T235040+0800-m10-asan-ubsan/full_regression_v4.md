# M10最终sanitizer全量回归v4

`ctest-v4.log`绑定`source_sha256.v2.txt`中的最终工具源码，使用ASan（LeakSanitizer关闭）
和UBSan在沙箱外执行全量CTest 20/20 PASS；其中M10为2/2，未出现sanitizer诊断。旧日志
保持不变，LeakSanitizer继续为`NOT RUN`。


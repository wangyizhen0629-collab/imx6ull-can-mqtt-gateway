# M1 ASan+UBSan 测试报告

GCC 11.4.0 使用 `-fsanitize=address,undefined` 完成构建；关闭当前环境无法支持的 leak
检测后，CTest 8/8 PASS，未报告 AddressSanitizer 或 UndefinedBehaviorSanitizer 错误。

LeakSanitizer 为 **NOT RUN**，不能据此宣称无内存泄漏。ARMv7、板端和硬件测试也均为
**NOT RUN**。

# M4 当前源码 sanitizer 尝试

结论：`NOT RUN`，不是 `PASS`。本机没有 WSL Linux 发行版，现有 MinGW GCC 6.3.0
在 ASan 链接时缺少 `libasan`，在 UBSan 编译时发生编译器内部错误。因此没有生成可运行
的 sanitizer 测试程序。

历史 `artifacts/20260830T205834+0800-m4-asan-ubsan/` 证明此前生产解码器在 Ubuntu
GCC 11.4.0 下 ASan+UBSan 全量 11/11 通过；本次没有修改生产解码器，但新增的语义测试
代码仍应在 Ubuntu clone 的受支持工具链上补跑，不能用历史结果替代当前源码运行结果。

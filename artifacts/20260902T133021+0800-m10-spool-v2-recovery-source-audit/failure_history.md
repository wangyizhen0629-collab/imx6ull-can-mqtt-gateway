# 失败历史

- ARMv7第一次configure：FAIL（exit 1）。命令遗漏本机必需的`IMX6ULL_SDK_ROOT`，CMake在
  生成前按toolchain合同拒绝；没有生成binary。原始输出保存在ARM artifact的
  `configure.log`。
- 纠正后使用仓库已有relocated SDK明确设置`IMX6ULL_SDK_ROOT`，在不同build目录重新
  configure并完成clean verbose rebuild。原失败没有删除、覆盖或改写为PASS。
- 最终Debug、标签CTest、ASan+UBSan和纠正后的ARM构建均无产品FAIL。


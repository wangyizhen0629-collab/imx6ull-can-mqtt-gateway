# M10 Ubuntu最终复跑补充报告

## 结果

- 分析器最终复跑：8/8 PASS。完整输出已保存在前一不可变run的
  `analyzer_unit_tests_v3.log`；本次终端复跑同样8/8、exit 0。
- 沙箱外全量CTest最终复跑：21/21 PASS，M10标签3/3，exit 0；前一不可变run的
  `ctest_unsandboxed_v3.log`保存同一当前源码的完整输出。
- 第一次生成器复跑：FAIL，exit 2；使用了不存在的旧脚本名。
- 第二次生成器复跑：FAIL，exit 2；找到真实脚本但遗漏必需的`--project`参数。
- 第三次使用完整项目路径：PASS，exit 0；三个canonical target一致。

两个失败均为复核命令错误，不是Keil build失败，也不改变Windows已有三个target rebuild
0 error/0 warning的事实。`docs/milestones/M10_STM32_PROFILE_DESIGN.md`已补齐可直接执行的
完整命令。前一run的错误命令记录和manifest均未改写。

最终`git diff --check`为exit 0；本次范围的RFC1918地址、私钥头及敏感赋值模式扫描0匹配。
前一run的40项manifest重新执行仍全部PASS。

硬件、部署和长时间测试仍全部`NOT RUN`；M10保持`NOT MET`。

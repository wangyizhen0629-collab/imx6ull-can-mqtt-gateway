# 最终审计

审计对象为准备提交的M10 Ubuntu复核修改和本run；审计在artifact manifest生成前执行。

- `git diff --check`：PASS，exit 0，无输出。
- `summary.json`：`python3 -m json.tool` PASS。
- `m4_replay_summary_v2.json`：`python3 -m json.tool` PASS。
- 敏感信息扫描：对本次源码、文档和artifact执行RFC1918地址、私钥头及
  password/passwd/secret/access token赋值模式扫描，0匹配，PASS。
- ARM binary忽略规则：`git check-ignore -v`命中`.gitignore:1:/build/`，PASS；binary
  未进入Git工作树提交范围。
- 最终当前源码CTest：21/21 PASS，M10 3/3；见`ctest_unsandboxed_v3.log`。
- 最终当前源码分析器回归：8/8 PASS；见`analyzer_unit_tests_v3.log`。
- Windows清单兼容复核：68/68 PASS；直接CRLF尝试的四次exit 1均保留。
- ARM `RelWithDebInfo`两次SHA一致，ELF/解释器/NEEDED/无RPATH检查PASS。
- 四组历史未跟踪目录仍存在，未暂存、覆盖、删除或提交。

审计不包含任何硬件执行；`not_run.md`中的项目保持`NOT RUN`，M10保持`NOT MET`。

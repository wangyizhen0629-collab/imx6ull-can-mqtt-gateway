# M4 Release 候选构建

配置成功，Release build 失败。GCC 11.4.0 在既有 M1 `lifecycle.c` 上报告 ignored
`write()` result，在 `log.c` 上报告可能的 `snprintf()` truncation；项目使用 `-Werror`，
因此停止。原始输出在 `build.log`。为遵守单 Milestone 边界，本轮没有修改这些历史模块，
也没有把该 run 改写为 PASS。

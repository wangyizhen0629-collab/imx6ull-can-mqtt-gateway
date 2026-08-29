# M2 主机开发 run

配置成功，warning-clean 构建失败。`build.log` 的真实编译错误是严格 POSIX feature 宏下
未公开 `SCM_TIMESTAMPNS` 名称。由于没有生成完整测试目标，CTest 为 `NOT RUN`。

该 run 保留为失败证据；后续修复使用新的 run，没有覆盖本目录。

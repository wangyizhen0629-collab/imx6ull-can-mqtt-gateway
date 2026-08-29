# M2 板端 loopback runner 主机静态检查

`sh -n tools/can/run_m2_board_loopback.sh` 返回 0，POSIX shell 语法检查 **PASS**。脚本
SHA256 为 `04c1052e232e976bec796cc59a15de0c9460e328f8b861416c764782751b5416`。

本机没有 `shellcheck`，因此 ShellCheck 为 `NOT RUN`；没有为此安装依赖。本 run 没有
操作主机或板端 CAN 接口，也不构成板端功能证据。

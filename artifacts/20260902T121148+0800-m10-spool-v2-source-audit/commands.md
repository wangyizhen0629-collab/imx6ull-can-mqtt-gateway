# 实际审计命令

前置命令按顺序执行：

```sh
git status --short --branch
git fetch origin
git pull --ff-only origin master
git rev-parse HEAD
git rev-parse origin/master
git switch -c m10-spool-v2-reclaim
```

HEAD与`origin/master`均为`6ee5d475f5451e6cba72f0041613009ed9fc9250`，pull为
fast-forward/no-op。最终源码审计命令及输出保存在同目录的`git-diff-check.log`、
`sensitive-scan.log`、`tracked-binary-audit.log`、`status-before-commit.log`和
`repository-final.log`。各构建/测试的实质命令见对应run的`commands.md`，日志末尾的
`COMMAND_EXIT_CODE`是实际退出码。

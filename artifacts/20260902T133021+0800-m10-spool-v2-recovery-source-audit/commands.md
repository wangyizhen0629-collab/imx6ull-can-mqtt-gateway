# 命令记录

本轮在仓库根目录执行以下类别的只读/本机命令；完整构建和CTest输出保存在另外三个唯一
artifact目录。

```sh
git status --short --branch
git switch m10-spool-v2-reclaim
git pull --ff-only origin m10-spool-v2-reclaim
git rev-parse HEAD
git diff --check
git diff --cached --check
ctest --test-dir build/20260902T133022-m10-spool-v2-recovery-host --output-on-failure
ctest --test-dir build/20260902T133022-m10-spool-v2-recovery-host --output-on-failure -L M7
ctest --test-dir build/20260902T133022-m10-spool-v2-recovery-host --output-on-failure -L M8
ctest --test-dir build/20260902T133022-m10-spool-v2-recovery-host --output-on-failure -L M9
ctest --test-dir build/20260902T133022-m10-spool-v2-recovery-host --output-on-failure -L M10
```

CTest在允许本机mock/loopback socket的环境执行；没有访问真实Broker、CAN或板端。


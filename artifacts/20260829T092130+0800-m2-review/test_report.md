# M2 最终一致性复核

- 全部 10 个 M2 artifact JSON 文件（包含本 review 的 manifest/summary）均可解析。
- `git diff --check` 通过，维护源码/文档没有行尾空白。
- worktree 范围只包含 M2 SocketCAN 源码/测试、M2 证据与所要求的状态文档；没有 STM32、
  DBC、部署、MQTT、spool 或 M3+ 源码改动。

该 run 只做一致性复核，不重复构建，也不包含 ARM 或板端结果。

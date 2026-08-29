# M2 最终一致性复核

- `git diff --check` 和维护文件行尾空白检查通过。
- 最终主机 run 的 26 个源码 checksum 全部重放通过，证明测试后没有再改源码。
- checksum 首次重放因 `script` 的 CRLF 记录格式导致文件名带 `\r` 而失败；保留失败日志，
  重试只清理输入流 CR，没有改动原证据。
- 全部 16 个 M2 artifact JSON（含本 run）可解析。
- worktree 没有 STM32、protocol、deploy、MQTT、spool 或 M3+ 源码改动。

该 run 不重复构建；最终构建/CTest 使用 `artifacts/20260829T092323+0800-m2-host-final/`
和 `artifacts/20260829T092406+0800-m2-asan-ubsan-final/`。

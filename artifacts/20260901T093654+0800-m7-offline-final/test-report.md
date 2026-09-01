# M7 离线实现最终证据

- run_id：`20260901T093654+0800-m7-offline-final`
- 时间：`2026-09-01T09:36:54+08:00`
- 起始 HEAD：`f185222bc685e54355b635388bf0094f7ec41b6e`
- 结果：离线实现与构建 **PASS**；M7 总退出门禁 **NOT MET**。

## 已执行

- Ubuntu Debug warning-clean构建 PASS；沙箱外全量 CTest 16/16 PASS，M7 标签3/3。
- ASan+UBSan warning-clean构建及全量 CTest 16/16 PASS；LeakSanitizer 为 `NOT RUN`。
- Buildroot GCC 7.5.0 ARMv7 warning-clean交叉构建 PASS；最终 ELF 为 hard-float、无
  RPATH/RUNPATH，SHA256 已记录。
- `test_spool` 实际覆盖：append+`fdatasync`、CRC、ACK state 原子替换、重开恢复、部分
  尾部截断、state CRC 损坏安全回退、内部损坏拒绝和单写者锁。
- `test_pipeline`额外覆盖从spool恢复出的初始gateway seq被producer实际采用。
- M7 validator 回归5/5 PASS，覆盖原始重复允许、按 `device_id + seq` 去重后完整、缺失
  拒绝、冲突重复拒绝及“必须观察到原始重复”约束。

## 未执行

- Windows Mosquitto Broker启停、断线/恢复、subscriber原始抓取：`NOT RUN`；项目所有者
  明确 Broker 位于 Windows，交由 Windows clone 后续执行。
- `SIGKILL` 后从同一 spool 恢复与去重完整性：`NOT RUN`；未启动测试进程，不推测结果。
- i.MX6ULL部署、真实物理CAN输入、目标持久存储介质上的掉电/崩溃恢复：`NOT RUN`；
  本次没有修改目标板、`can0`、固件、Broker或系统服务。
- M8 external-loop/epoll及后续功能：未实现、未测试。

因此本run只允许描述为“M7源码、主机单元/消毒器及ARMv7交叉构建通过”，不能描述为
断线补传、崩溃恢复或M7总门禁通过。

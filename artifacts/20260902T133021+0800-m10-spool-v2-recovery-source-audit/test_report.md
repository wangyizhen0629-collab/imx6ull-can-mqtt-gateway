# M10 spool v2恢复纠正源码审计

- 时间：2026-09-02（Asia/Shanghai）
- 基线：`da122011119821be4cc7a7f7449f810f825dd96b`
- 分支：`m10-spool-v2-reclaim`
- 范围：只审计M10 spool v2恢复路径；未连接或修改真实板端、CAN、Broker、STM32。

## 结论

Windows审查指出的data-before-state缺口已纠正：加载GST2后保留原持久
`write_segment/write_offset`；若扫描拟接纳游标后的完整记录，先对原write segment执行
`fdatasync`，成功后才推进内存游标并提交新state。同步或close失败均使open失败，磁盘
state不推进。

定向测试使用子进程`_exit`且不调用用户态flush/close，分别制造部分segment和恰好填满
segment的完整尾记录。恢复segment sync故障时open返回I/O错误、state 112字节逐字节
不变；随后无故障reopen安全接纳记录。满segment场景的新state游标为下一segment/offset 0，
但恢复提交state前不会创建下一segment。

源码与设计审计：PASS。最终`git diff --check`、敏感信息扫描、依赖/二进制跟踪审计及
artifact manifest结果见本目录的对应日志；提交SHA在提交后由Git事实确定，不回填进
manifest覆盖范围。

## 限制

本次离线实现与测试不能证明真实掉电、文件系统/介质flush兑现、在线写放大、CPU/RSS、
性能或长期稳定性。尤其pending=0时ACK可能每批滚动并删除小segment；1秒批次可能每秒
产生create/sync/delete元数据活动，必须留待另行批准的120秒板端预演量化。


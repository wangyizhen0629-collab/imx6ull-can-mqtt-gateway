# M5 最终一致性审计

结论：审计 `PASS`，M5 实现完成，但退出门禁为 `NOT MET`。复核结果如下：

- M5 新增/修改生产源码 SHA256 与最终主机 artifact 一致；
- Ubuntu warning-clean 全量 CTest 12/12和 M5 专项输出存在且为 PASS；
- ASan+UBSan 全量 CTest 12/12和 M5 专项输出存在且为 PASS，LSan 明确 NOT RUN；
- ARM binary 当前 SHA256 与交叉构建 artifact 中的
  `567079d01f4fb1e682a959cd01bac3709e4062f42c1f18903596dc47181d0a01` 一致；
- 权威文档统一记录“实现完成、目标板测试 NOT RUN、M5 门禁 NOT MET、M6 未开始”；
- `git diff --check` 退出码为0。

环境未安装 `jq`，首次 JSON 工具检查原样保留在 `json_parse.log`；随后使用 Python 3
标准库成功解析11个当时已存在的 M5 JSON 文件。该 fallback 不修改证据内容。

本审计没有部署 binary、连接目标板、修改 `can0`、启动/终止板端进程或接触 Broker。
真实 i.MX6ULL 物理 CAN 基准、慢消费者过载和板端 `SIGTERM` 继续为 `NOT RUN`，不能
从主机或交叉构建结果推测。按阶段门禁规则，本轮在 M5 停止。

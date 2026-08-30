# M4 最终一致性审计

显式限定六个 M4 测试 run 后，12个 `manifest.json`/`summary.json` 均可解析。普通主机、
ASan+UBSan 和 ARM run 的三份源码 checksum 清单共30项全部匹配当前 M4 源码；
`git diff --check` PASS。

`git_state.txt` 保留最终工作区清单。新增功能文件只涉及 vehicle protocol/decoder、DBC、
黄金向量和检查器；没有新增 M5 生产者--消费者、mock sink、MQTT、spool 或部署实现。
本审计没有访问目标硬件或改变任何运行状态。

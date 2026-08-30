# M4 首次最终审计

审计脚本错误地假定六个测试 run 各有三个 JSON，预期18个；实际每个 run 是
`manifest.json`、`summary.json` 和 Markdown 报告，共12个 JSON。数量断言先失败，尚未
解析 JSON 内容。原日志保持不变，后续使用新 run 和显式目录列表重试。

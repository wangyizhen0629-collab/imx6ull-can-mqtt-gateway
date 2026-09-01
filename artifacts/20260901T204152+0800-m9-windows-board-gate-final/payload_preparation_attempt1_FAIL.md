# 私有配置载荷准备 attempt 1

结果：**FAIL（未部署）**

首次生成后的独立键计数发现 `device_id`、`mqtt_topic`、`spool_path` 各出现两次。原因是
PowerShell `switch` 内的 `continue` 没有跳过外层逐行循环，替换行和原行都被输出。

该失败载荷保留在 Git 忽略的 `private_raw/incoming/gateway.conf`，没有传输到目标板，
也没有修改任何目标文件。后续只允许使用经结果侧逐键计数、值对照和 SHA256 复核通过的
`gateway.v2.conf`。

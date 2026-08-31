# M6 validator 严格整数回归报告

- Run ID：`20260831T212826+0800-m6-validator-strict-int`
- 测试提交：`68ec4699df91fd837655996b014eb40a6e15b834`
- validator修复提交：`917253e6a514ea0e65df3de07accf7a9f284f0c4`
- 结论：**PASS**
- M6总退出门禁：**NOT MET**

## 修复内容

validator不再用 `isinstance(value, int)` 判断协议整数，而是使用严格JSON integer语义。
`batch_seq`、`record_count`、`first_seq`、`last_seq`、record `seq` 和DLC中的
`true`/`false` 会被拒绝，`8.0` 也不能充当整数DLC。

新增不访问网络的 `test_mqtt_validator`，作为M6 unit test注册到CTest。8项用例覆盖：

- 两个多记录batch的全局连续seq；
- 跨batch gap和duplicate拒绝；
- 布尔batch seq、record seq、record_count、first/last seq拒绝；
- 浮点DLC拒绝。

## 实际测试结果

- fresh CMake配置退出0，warning-clean build退出0；构建输入为已归档过的私有x86_64
  libmosquitto 2.0.11，所选路径和SHA256已保存；
- 严格整数回归8/8 PASS，退出0；
- 历史1000个单记录batch重放PASS：batch seq和gateway seq均为1..1000；
- 板端smoke首个多记录batch重放PASS：1个batch、111条records、gateway seq 1..111；
- 同一smoke输入故意设置 `expected-batches=2`，退出1并报告原始batch数1，符合预期；
- 沙箱内CTest 13/14，唯一失败为既有PF_CAN socket权限限制；经批准在沙箱外重跑同一套
  CTest，14/14 PASS，其中M6的 `test_mqtt_sink` 和 `test_mqtt_validator` 均PASS。

测试开始前已提交修复和artifact行尾规则，测试时tracked worktree clean。`git_head.txt`
和 `git_blob_ids.txt` 可跨clone确认被测提交及4个源码/测试blob；工作树SHA256只描述本次
Ubuntu实际输入，不再把它冒充跨Windows/Linux恒定的文本文件哈希。

## 证据边界

开发期首次快速回归的 `record.seq=true` 夹具同时把 `first_seq/last_seq` 设成布尔值，
因此实际在更早的元数据检查处被拒绝。该夹具随后被修正为只有record seq是布尔值；
首次快速运行未放入本final run，不计作正式证据，最终独立run的8项结果完整保存。

本run没有启动或控制Broker，没有访问或修改板端，没有采集新的1000个batch，也没有执行
绝对UTC/timestamp或M7测试。它只关闭validator整数语义和回归覆盖问题；正式M6局域网
1000-batch门禁仍为 **NOT MET**。

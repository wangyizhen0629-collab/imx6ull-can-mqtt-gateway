# M6 validator 多记录 batch 回归报告

- Run ID：`20260831T205108+0800-m6-validator-multirecord`
- 基线提交：`7ab18cce43617000e8846998c0fd3aca8b67bdfb`
- Python：3.11.7
- 结论：**PASS**

## 源码行为

validator继续要求 `batch_seq` 精确等于 `1..expected_batches`。每个 batch仍检查 schema、
device ID、record_count、first_seq、last_seq、DLC、data长度和decoded_payload长度。

所有 batch 的全部 records 按输入顺序展开后，gateway seq改为全局验证
`1..last_gateway_seq`，不再错误地要求record数量等于batch数量。因此原单记录格式与物理
CAN多记录batch均受支持。

## 实际测试

1. 历史loopback `subscriber.jsonl`：expected-batches=1000，退出码0；1000个batch、
   batch seq 1..1000、gateway seq 1..1000，PASS。
2. board smoke `subscriber.json`：expected-batches=1，退出码0；1个batch含111条records、
   batch seq 1、gateway seq 1..111，PASS。
3. 同一board smoke输入故意设置expected-batches=2：退出码1，错误为
   `raw batch count 1 != 2`，符合预期。

两个正例stderr均为空，负例stdout为空。完整stdout、stderr和退出码均保存在本目录。

## 边界

这是对已有文件的validator回归，不是新的Broker、板端、物理CAN或1000-batch运行。
board smoke的absolute UTC/timestamp correctness仍为 **NOT RUN**，subscriber精确退出码
仍为 **NOT AVAILABLE / NOT RUN**。本结果不改变M6总门禁NOT MET，也不涉及M7。

两个先前的编排失败run按不可覆盖规则保留为FAIL；它们没有启动validator，不能计入测试
通过数。

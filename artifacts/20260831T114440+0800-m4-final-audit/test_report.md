# M4 最终一致性审计

结论：`PASS / MET`。本 run 只读复核当前 M4 范围，没有修改目标板、CAN 接口、固件、
进程或 Broker。新证据 JSON 均可解析，三份物理原始文件 SHA256 与 manifest 一致；
DBC/42条向量、C 解码器完整6660帧语义模型以及物理6660帧逐帧检查均通过。

当前权威文档已统一写明 M4 于2026-08-31完成，并保留以下边界：当前 Windows
ASan/UBSan `NOT RUN`；新增解码器在 i.MX6ULL 实时运行 `NOT RUN`；完整原始 Keil
Build/Download 输出未归档。历史旧模型 PASS/FAIL artifact 均保持原义。

源码和文档范围的 `git diff --check` 退出码为0。暂存原始证据后，完整
`git diff --cached --check` 会报告 `can_before.txt`/`can_after.txt` 原始命令输出自带的
行尾空格；这些空格不能为了通过样式检查而改写。仓库以 `.gitattributes -text` 对三份
实物原始文件禁用 CRLF 转换，并以 manifest SHA256 复核逐字节内容。本审计不处理或删除
工作树中项目所有者已有的无关未跟踪 STM32/CMSIS 文件。

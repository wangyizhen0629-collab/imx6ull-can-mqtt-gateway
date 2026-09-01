# 证据来源说明

- 原始输入：工作区`tmp/evidence.txt`，包含操作者从板端终端复制的完整早期会话。
- Codex读取时初始版本：9948 bytes，SHA256
  `4747e89cfccd275f7b60d64add02b62ed2597f84d62c25f3b3fb745f640a707c`，270行。
- 操作者第一次追加后：14127 bytes，SHA256
  `a444f5d50fab2bd5293a10e2671103f7a1ee532ce70515f87be87ca6273e3fd4`，366行；该版本仍缺
  CAN恢复、最终start和最终稳定性三段。
- 经操作者明确要求“直接帮我加进去”，Codex只在文件末尾追加操作者此前在本会话中
  粘贴的三段终端文本，没有改写原有内容。追加段明确标记为conversation paste
  reconstruction，不声称是字节级MobaXterm导出。
- 最终输入的bytes、SHA256和行数由`capture_evidence.ps1`重新计算并记录；原字节只复制到
  Git忽略的`private_raw`，提交内容只使用脱敏副本。

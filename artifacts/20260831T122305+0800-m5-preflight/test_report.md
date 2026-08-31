# M5 前置门禁审计

结论：`PASS / M4 MET`。开始 M5 实现前，工作区位于提交
`2249a45142e0210b7a5aaed6c65798cb07402aab` 且无本地修改。Ubuntu GCC 11.4.0
warning-clean 构建通过；首次全量 CTest 因沙箱拒绝创建 `PF_CAN` socket 而保留为
FAIL，经授权在不修改任何 CAN 接口状态的条件下复跑后 11/11 PASS。

当前仓库源码再次通过3条 DBC 消息和42条黄金向量检查。M4 三份物理原始文件的 SHA256
与 manifest 一致，60秒捕获的6000/600/60帧再次逐帧审计通过，payload/counter/XOR/
spare-bit 差异均为0，归档 CAN 错误计数增量为0。因此 M4 的窄范围退出条件满足，可以
进入本次已由项目所有者明确授权的 M5。

旧 Windows `sha256.txt` 中源码相对路径从 artifact 目录执行时会越过 Ubuntu 仓库根目录，
且部分源码哈希记录的是 CRLF 字节；该可移植性限制不改写旧证据。本次以原始 CAN 文件
哈希和当前源码重新执行行为检查为准。本 run 未连接目标板，未修改 `can0`、固件、进程
或 Broker 状态。

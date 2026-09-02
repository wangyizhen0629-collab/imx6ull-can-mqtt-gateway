# M10 spool v2设计与源码审计

- 时间：2026-09-02（Asia/Shanghai）
- 基线：`6ee5d475f5451e6cba72f0041613009ed9fc9250`
- 分支：`m10-spool-v2-reclaim`
- 范围：仅M10离线纠正性准备；没有执行M11、硬件、板端、CAN、Broker或长时间测试。

## 结论

设计冻结、分段回收、容量保护、sequence reservation、可配置group commit、配置/summary
接线和离线故障注入已经实现。legacy仍是默认格式且仍逐条同步；v2必须显式选择并使用
全新目录。任何未ACK记录不得被回收或覆盖，未知/缺失/乱序格式和内部CRC损坏fail closed。

源码与测试证据分为两个提交边界：提交A只加入分段回收并保留逐条`fdatasync`；提交B
加入group commit、集成测试、文档和最终证据。包含本artifact的提交不能在自身内容中
保存其最终SHA；最终SHA只由提交后的`git log`、远端ref和交接报告三方核对，不做自引用。

## 审计点

- GSP2记录80字节、CRC32；GST2 state 112字节、CRC32和generation。
- 生产segment固定65536条，即5242880字节；名字是20位递增十进制编号。
- ACK state先tmp完整写、`fdatasync`、rename、父目录`fsync`；仅全ACK segment可删，
  删除后再次`fsync`目录。
- state在所有旧segment删除后仍保留last allocated/fence/last ACK/next batch及cursor；
  gateway、batch和segment编号均不复用。
- v2容量默认268435456字节，合法范围5242880～68719476736；满时返回
  `GATEWAY_ERROR_CAPACITY`，不覆盖未ACK记录。
- 默认同步参数为1条/1000 ms，保持legacy/严格语义；M10候选128条/1000 ms必须显式配置。
- group模式在记录阈值、时间阈值、segment滚动、publish候选前和正常关闭同步；Broker
  离线poll仍执行时间阈值。sync失败传播并令实例fail-stop。
- 新快照公开physical/pending bytes、segment count/reclaimed、sync count/failures和
  unsynced records。

## 开发失败历史

1. 第一次开发配置没有显式传入本机已有的libmosquitto头文件/库路径，CMake因找不到依赖
   失败。随后使用已存在的`/tmp/imx6ull-m6-mosquitto-root`输入成功；没有下载或安装依赖。
2. 一次受限沙箱M7标签回归因PF_INET listener权限被拒而失败。它不是Broker或产品失败；
   相同源码在允许本机loopback socket的执行权限下M7 4/4 PASS。

以上两次发生在正式唯一run创建前，原始终端输出没有预先写入artifact，因此无法补造
“完整原始日志”。本文件保留实际发生、原因和纠正动作；最终正式run的所有命令输出和退出
码均已完整归档。没有删除或覆盖失败artifact。

## NOT RUN

LeakSanitizer、真实掉电、真实ext4目录/设备掉电语义、板端容量/写放大/CPU/RSS、旧M9
spool迁移或清理、binary板端执行、STM32烧录、三档短candump、120秒板端预演、短端到端
对账、500/1000帧/s各30分钟、20轮Broker中断和111帧/s 24小时全部`NOT RUN`。

M10总门禁保持`NOT MET`，不产生性能、CPU/RSS、时延、掉电零丢失或长期稳定性结论。

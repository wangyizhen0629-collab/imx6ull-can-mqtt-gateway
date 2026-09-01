# M9手动post-boot补充门禁报告

## 结论

结合基础run `artifacts/20260901T204152+0800-m9-windows-board-gate-final/`和本补充run，
M9 BusyBox进程监督门禁为 **MET**，M10 **未开始**。

本run证明唯一一次受控reboot确实产生新boot ID，BusyBox PID 1在没有人工start/restart/HUP
的前提下从inittab自动拉起唯一supervisor。重启后的现有`can0`最初为DOWN/STOPPED，
所以真实gatewayd无法保持；这段观察不伪装为PASS，也不武断声称唯一退出原因。操作者在
新增明确授权后只按已验证基线恢复CAN，再执行一次受控start。最终supervisor PID 337和
child PID 9951超过60秒保持不变，身份、父子关系、SHA、动态库映射和1/1唯一性均通过。

## 本run结果

| 项目 | 结果 | 真实证据 |
| --- | --- | --- |
| reboot真实性 | PASS | boot ID由基础run的`d9f9b72d-c59c-4cc8-9201-1e9ac3da0e39`变为`0abefcf0-9d85-4a4b-b335-f339b33b8db4` |
| BusyBox init自动拉起supervisor | PASS | PID 337、PPID 1、cmdline `/bin/sh /etc/init.d/gatewayd supervise`；操作者确认此前未人工start/restart/HUP |
| 部署文件身份 | PASS | inittab、init/env/config、binary和lib权限/SHA均与基础run一致；binary SHA为预期值 |
| post-boot外部CAN前置条件 | 初始不可用，后经新增授权恢复 | DOWN/STOPPED、统计0后按既有基线恢复为UP/LOWER_UP、ERROR-ACTIVE、500000 bit/s、berr 0/0 |
| 安全停止 | PASS | 外部条件不可用时受控stop exit0，disabled、唯一supervisor、child0、测试进程0 |
| 最终受控start | PASS | `action=start result=started`、exit0；5秒status running exit0 |
| 最终唯一进程 | PASS | supervisor/child为1/1，other gatewayd和test process均为0 |
| 最终身份 | PASS | child PID 9951、PPID 337、exe/cmdline正确，SHA为预期值并映射固定libmosquitto |
| 最终稳定状态 | PASS | 60秒前后child PID均为9951，最终status exit0，disabled不存在 |
| 最终CAN只读状态 | PASS | UP/LOWER_UP、ERROR-ACTIVE、500000 bit/s、berr 0/0；只作功能状态，不计算性能 |

## 组合门禁

- BusyBox init开机自动启动：本run PASS；
- 唯一supervisor和唯一gatewayd：本run最终PASS；
- 受控restart更换PID：基础run PASS（14335→15095）；
- gatewayd异常退出后自动拉起：基础run PASS（11348经一次SIGKILL后→14335）；
- 快速失败阈值和cooldown：基础run在目标BusyBox 1.31.1 ash隔离fake中PASS；
- 最终服务正常且无重复进程：本run PASS。

因此六项均有真实证据。M9仅在“BusyBox进程监督与恢复”范围内关闭为MET；不得把手工恢复
CAN、1970时钟或未验证Broker交付扩写成完整产品无人值守启动、性能或可靠性结论。

## 证据限制

`operator_terminal_evidence.redacted.txt`由操作者终端文本与明确标记的会话补录组成，不是
字节级MobaXterm导出。原始登录提示符仅存在于Git忽略的`private_raw`，公开文件完成登录
标识、目标标识、私有IPv4和凭据字段扫描。板端时钟无效；操作顺序使用boot ID和uptime。

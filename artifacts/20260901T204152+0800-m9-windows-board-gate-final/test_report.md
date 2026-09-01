# M9 Windows真实i.MX6ULL板端门禁报告

## 结论

M9总门禁：**NOT MET**。M10：**未开始**。

本轮已真实通过binary/目标库硬门禁、备份与安装、PID 1 HUP启动、唯一生产
supervisor/child、受控restart、一次子进程SIGKILL自动拉起，以及目标BusyBox 1.31.1
ash隔离重启风暴cooldown。唯一一次reboot命令发出后目标SSH持续不可用，未取得新boot
ID；因此开机自动启动和最终服务状态没有本轮证据，不能将M9改为MET。

## 门禁结果

| 项目 | 结果 | 本轮事实 |
| --- | --- | --- |
| Windows clone/pull/HEAD | PASS | 跟踪文件0改动；ff-only pull无更新；HEAD `17d698e16d5cfe8e86fa0b4fdc47ea9515df65ab`且包含要求提交 |
| ARM binary身份 | PASS | 板端104860 bytes；SHA256 `6e8729417b3dc40c10a413459de5eca9be43ce58dfcc8a3b12e91f5c8d7ef958`；ARMv7 hard-float、解释器/NEEDED正确、无RPATH/RUNPATH |
| 目标libmosquitto | PASS | SHA256 `b32c8ac4defb2b2920fba2e42f263869508c42e3c1719440db37ffc8d8c2f636`；ARM ELF；`ldd`完整解析；version执行exit0 |
| 备份和安装 | PASS | ext4本轮备份/rollback已保存；`/opt`私有binary/lib及四个授权`/etc`文件权限/hash通过；inittab唯一respawn行；无`S??gatewayd` |
| PID 1 reload启动 | PASS | HUP exit0；精确1 supervisor/1 gatewayd child；PID 11337/11348稳定；脚本/config/binary/库映射/PPID通过 |
| 唯一进程分类 | PASS（一次证据脚本FAIL后修正） | supervisor shell的comm也为`gatewayd`；首版comm计数错误为2并保留FAIL。v2以cmdline和exe+SHA+PPID分类为1/1，不相关gatewayd为0 |
| gatewayd异常退出恢复 | PASS | 只对核验子PID 11348执行一次SIGKILL，exit0；同一supervisor 11337拉起不同PID 14335，稳定1/1 |
| 受控restart | PASS | `/etc/init.d/gatewayd restart` exit0；旧子PID 14335消失，新PID 15095稳定；supervisor不变；TERM路径/handler位已保存 |
| 快速失败cooldown | PASS | BusyBox 1.31.1 ash隔离fake：3次快速失败触发storm；1秒内启动数仍为3；2秒cooldown后第4次启动并恢复；测试残留0 |
| 真实reboot后开机自动启动 | NOT RUN | reboot前marker和旧boot ID已持久化；reboot命令只发送一次。48次恢复探测及最终短探测均exit255，未读取新boot ID |
| 最终唯一supervisor/gatewayd和服务状态 | NOT RUN | 目标SSH不可达，无法执行post-boot身份、稳定性和最终1/1核验 |
| post-boot CAN/Broker只读状态 | NOT RUN | 目标SSH不可达；禁止自行修改网络、CAN或Broker |
| rollback | NOT RUN | reboot前没有重复/风暴/启动失败，故未触发回滚；reboot后目标不可登录，若需要也无法执行。回滚脚本已在持久化备份目录准备 |

## 关键执行证据

- `board_binary_hard_gate.public.txt`：binary size/SHA/ELF/interpreter/NEEDED/RPATH完整输出；
- `board_library_predeploy_gate.public.txt`：库候选、目标动态加载、PID 1、BusyBox、文件系统、安装前文件/进程/CAN/socket；
- `stage_payload.public.txt`和`board_install.public.txt`：本地/目标payload SHA、权限、备份、安装前后完整inittab和动态依赖；
- `board_reload_start.public.txt`及`reload_classification_attempt1_FAIL.md`：HUP真实启动和首版comm分类错误；
- `board_reload_classification_v2.public.txt`：精确1/1分类与稳定PID；
- `board_sigkill_recovery.public.txt`：SIGKILL前后身份和不同PID；
- `board_controlled_restart.public.txt`：restart前后身份、TERM路径、动作exit和不同PID；
- `board_storm_test.public.txt`：完整BusyBox ash `-x` trace、阈值/cooldown/恢复和清理；
- `board_pre_reboot.public.txt`：reboot前boot ID、PID、inittab、marker、spool、CAN/socket；
- `reboot_recovery_poll.v2.public.txt`与`final_reachability_probe.public.txt`：无第二次reboot的48次恢复探测和最终exit255。

## 保留的失败尝试

1. 首版私有配置生成器产生3个重复key；独立计数发现后未上传，v2的14个key各一次。
2. binary硬门禁首行因Windows stdin BOM产生一条无害shell错误；所有门禁命令和最终exit0，后续封装去除BOM。
3. reload首版只按comm计数，把shell supervisor和真实binary合计为2；精确分类v2 PASS，首版FAIL未覆盖。
4. 首版reboot Windows控制器在预期SSH断线时因PowerShell stderr终止；reboot命令不重发，改用只读恢复轮询。
5. 恢复轮询首版公开脱敏遗漏截短IPv4残片；原字节移入Git忽略的private quarantine，v2精确endpoint及2～4段点分数字扫描均为0。
6. 最后一次stdin post-boot脚本在SSH管道关闭前未执行；随后短SSH探测exit255。
7. 最终审计首版把本机缺少Git Bash计为shell失败、把每个文档未重复run ID计为失败，且
   CRLF使两个空凭据行被正则误合并；首版exit1和manifest均保留。v2按实际文档职责和
   逐行凭据重新审计；shell语法解析明确为`NOT RUN`（工具不可用），不伪装成PASS。
8. 首次索引级`git diff --cached --check`发现板端原始输出的行尾空格/EOF空行；28个公开
   文件及随后1个审计记录的原字节均保存到Git忽略目录，公开副本只做空白规范化并保存
   旧/新SHA映射。最终以v4 manifest、敏感扫描和索引检查为准。

## 外部条件与限制

- reboot前目标wall clock仍为1970，只记录原始板端时间，不声称正确UTC，也不计算性能。
- reboot前`can0`只读状态为UP/ERROR-ACTIVE且当前berr tx/rx为0/0；累计error-warn/
  error-pass历史值存在。本轮未修改CAN，不能用这些快照生成可靠性结论。
- 配置的Broker连接只观察到SYN-SENT；本轮没有启停/修改Broker，也没有验证MQTT交付。
- spool大小变化来自服务运行中的持久化输入，只作为文件存在/持久化事实，不用于吞吐、
  时延、压力、寿命或持续运行结论。
- 未执行性能、压力、长时间或24小时测试；未修改网络、STM32、固件、系统库或依赖包。

真实endpoint、私有配置及完整raw输出只在Git忽略的`private_raw/`。公开证据只保留脱敏
文本和raw/redacted SHA256映射。

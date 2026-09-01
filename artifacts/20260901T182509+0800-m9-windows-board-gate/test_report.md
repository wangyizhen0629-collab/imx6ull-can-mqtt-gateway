# M9 Windows/真实目标板门禁续跑

## 判定

- 本 run：**NOT RUN（部署与功能门禁）**
- M9 总门禁：**NOT MET**
- M10：**未开始**

本轮已成功从 Windows 通过既有 SSH/MobaXterm 路径访问真实 i.MX6ULL，并完成只读前置
核验；但指定的 Ubuntu 构建产物 `build/m9-arm-cross/gateway/gatewayd` 无法从 Windows
侧认证读取，在 Windows clone 和目标板上也没有找到预期 SHA256 的 M9 binary。因此
未能从真实源文件重新计算
`6e8729417b3dc40c10a413459de5eca9be43ce58dfcc8a3b12e91f5c8d7ef958`。
按照“若不一致或不可用立即停止”约束，本轮在任何 staging、`/etc`、init、reboot 或
进程信号之前停止，不能沿用预期 hash 作为实测结论。

## 已实际完成的只读检查

1. Windows clone 在无已跟踪改动的前提下以 `git pull --ff-only origin master` 快进，
   当前 HEAD 为 `e673856f8ce789442b63464c1bc9753c4d97f619`；1305 个原有未跟踪文件未被清理或
   覆盖。正式本地预检再次 pull 为 exit 0、Already up to date。
2. 目标登录 exit 0；PID 1 的 `comm=init`、`cmdline=init`、`exe=/sbin/init`。`ldd`证明
   它链接 `/lib/libbusybox.so.1.31.1`；目标库字符串实测包含 `BusyBox v1.31.1`、
   `respawn` 和 `reloading /etc/inittab`。这比仅依据 `/etc/inittab` 注释更强，也纠正了
   “目标一定存在 `/bin/busybox` 命令”的错误前提：本镜像使用 standalone applet。
3. 修改前 `/etc/inittab` 权限为 0644，SHA256 为
   `77676429d4b24e2a93b8d7f7c3a9a35a37785600ab359e1c75ec6c8da11d1380`；未发现
   gatewayd respawn 项。四个授权 M9 文件中只有 `/etc/inittab` 存在，其他三个均缺失。
4. 目标上的 gatewayd 候选都属于旧 M6～M8 SHA；没有预期 M9 SHA 的候选。目标有
   `libmosquitto.so.1` 候选，但由于精确 M9 binary 缺失，不能执行本次 binary 的 ELF、
   `ldd` 和配置组合门禁。
5. 板端 wall clock 仍为 1970，仅能用作命令顺序证据，不能声称正确 UTC 或计算性能。
6. 结束只读审计 exit 0：本 run 的 `/tmp`/`/var/lib` staging 均不存在，
   `supervisor_count=0`、`gatewayd_count=0`；`/etc/inittab` hash 未变，另外三个 M9 文件
   仍缺失，没有遗留本 run 测试进程。`can0` 和现有 Broker 只读观察，未执行任何修改。

## 必测项结果

| 门禁项 | 结果 | 原因 |
| --- | --- | --- |
| Ubuntu M9 binary 安全转交及重新计算 SHA256 | NOT RUN | Ubuntu SSH 端口可达，但 Windows 侧没有可提供的公钥且 BatchMode 认证被拒；未猜测或提取凭据 |
| 非系统目录部署、权限、ELF、动态依赖和配置核验 | NOT RUN | 精确 M9 binary 不可用，部署前置条件失败 |
| 备份并安装四个授权 `/etc` 文件 | NOT RUN | 在首次目标写入前停止 |
| BusyBox init reload 或一次受控 reboot | NOT RUN | inittab 未修改，无可验证的新服务 |
| 真实开机自动启动 | NOT RUN | 未安装服务 |
| 唯一 supervisor 和唯一 gatewayd | NOT RUN | 未安装服务；结束审计为零/零而非门禁要求的一/一 |
| 受控 restart 更换 PID | NOT RUN | 未安装服务 |
| gatewayd SIGKILL 后自动拉起 | NOT RUN | 未安装服务 |
| 目标 BusyBox 1.31.1 隔离 fake gateway 阈值/cooldown | NOT RUN | binary 前置门禁失败后禁止继续 staging；没有用真实 gatewayd 制造风暴 |
| 最终服务正常且无重复进程 | NOT RUN | 服务未安装；仅证明无遗留测试进程 |

## 证据边界与恢复条件

完整目标输出位于 Git 忽略的 `private_raw/`，提交内容只含脱敏副本及 raw/redacted
SHA256 映射。要继续 M9，必须先让 Windows 以已有、明确授权的认证方式读取 Ubuntu
构建侧精确文件，重新计算 SHA256 并匹配预期值；不得用旧 M8 binary、目标旧文件或
预期字符串代替。满足该条件后必须新建另一个唯一 artifact，再从非系统 staging、
备份和部署重新开始。M9 在真实开机、restart、异常拉起、cooldown 和最终一/一进程
状态全部 PASS 前保持 **NOT MET**。

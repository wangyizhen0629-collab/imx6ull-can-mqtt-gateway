# BusyBox 部署

M9选择BusyBox `inittab`的`respawn`动作承载常驻supervisor，不使用systemd。init会在
`sysinit`/`wait`动作完成后启动respawn项；supervisor再以前台子进程方式启动
`gatewayd --config /etc/gatewayd/gateway.conf --run-mqtt`。这样不会由rcS和inittab各启
一份进程。

## 仓库文件与目标路径

| 仓库文件 | 建议目标路径 | 建议权限 |
| --- | --- | --- |
| `deploy/init.d/gatewayd` | `/etc/init.d/gatewayd` | `root:root 0755` |
| `deploy/config/gatewayd.env.example` | `/etc/default/gatewayd` | `root:root 0644` |
| `gateway/config/gateway.conf.example`的私有副本 | `/etc/gatewayd/gateway.conf` | `root:root 0600` |
| ARMv7 `gatewayd` | `/usr/bin/gatewayd` | `root:root 0755` |
| 目标`libmosquitto.so.1`及其真实文件 | 目标动态加载器可见的私有库目录 | 按M6～M8已验证布局 |

`gatewayd.respawn`不是BusyBox可自动include的配置文件。部署人员必须把其中唯一的
`null::respawn:/etc/init.d/gatewayd supervise`合并到`/etc/inittab`，并先确认不存在同
服务的重复respawn项。禁止同时把脚本命名为rcS自动执行的`S??gatewayd`。

## 恢复与节流语义

- `supervise`始终以前台方式运行，子进程异常或正常退出都会重新拉起。
- 连续快速退出达到`GATEWAYD_RESTART_LIMIT`后，先等待`GATEWAYD_COOLDOWN_SEC`，避免
  配置错误或依赖故障造成无间隔重启风暴；稳定运行达到`GATEWAYD_STABLE_SEC`后清零计数。
- `restart`向已核验的supervisor发送HUP，由supervisor向子进程发送TERM并等待退出，再
  启动不同PID；受控restart不计入快速失败。
- `stop`创建`/var/run`下的临时disabled标记并停止子进程，supervisor保持存活，避免
  BusyBox init立刻respawn；`start`移除标记并恢复子进程。该标记在重启后自然消失。
- PID信号只在`/proc/<pid>/cmdline`确认PID属于同一路径的supervisor后发送。运行目录和
  env文件必须由root控制。

## 安装与板端门禁

先在目标板非系统测试目录核对binary、库、脚本和配置，再取得批准后备份并修改`/etc`。
修改后可向PID 1发送HUP使BusyBox init重新读取inittab，或执行一次受控reboot；具体方法
必须先在目标BusyBox 1.31.1上确认并归档。M9板端门禁至少保存：

1. 修改前后的`/etc/inittab`、文件权限、SHA256、BusyBox版本和init PID/comm；
2. 一次真实开机后自动启动，且只有一个supervisor和一个gatewayd；
3. `restart`替换子PID并完成优雅退出；
4. 只对已核验的本次gatewayd执行一次异常终止，观察新PID自动拉起；
5. 使用测试配置触发快速失败，证明达到阈值后进入冷却而不是高速循环；
6. 测试结束后的进程、CAN、Broker、配置和inittab状态。

写入`/etc`、修改inittab、reboot、发送进程信号、修改CAN/Broker状态前均必须取得明确
批准。无法执行真实目标板步骤时必须标记`NOT RUN`，主机BusyBox测试不能替代开机门禁。

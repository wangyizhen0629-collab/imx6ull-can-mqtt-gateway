# M9 真实目标板门禁

## 结果

`NOT RUN`

## 未执行项

- 将M9 ARMv7 binary、目标库、私有配置和supervisor部署到i.MX6ULL；
- 备份并修改目标板`/etc/inittab`、`/etc/init.d`、`/etc/default`和`/etc/gatewayd`；
- 让目标BusyBox 1.31.1 init重新加载inittab或执行受控reboot；
- 证明真实开机后只有一个supervisor和一个gatewayd；
- 对核验后的目标gatewayd执行受控restart和一次异常终止并观察自动拉起；
- 在目标BusyBox 1.31.1上触发快速失败并验证冷却；
- 保存板端CAN/Broker/进程/配置/inittab前后状态和完整退出码。

## 原因

当前Ubuntu会话没有可用的目标板登录端点、凭据或已验证的Ubuntu到i.MX6ULL传输路径，
不能实际操作目标板。真实地址和凭据也不得从私有历史证据复制进仓库。上述操作还会修改
`/etc`、init、进程和reboot状态，必须在具备目标访问路径的环境中按`deploy/README.md`
逐项执行并保留新的唯一artifact。

## 所需条件

1. 项目所有者提供/使用已验证的Windows MobaXterm或Ubuntu到目标板访问路径；
2. 在执行前再次确认本次允许备份和修改指定`/etc`文件、向init发送reload或reboot、
   控制只属于本run的gatewayd进程；
3. 提供脱敏后可提交的板端原始输出、文件SHA256、进程树和重启前后记录；
4. CAN和Broker若不处于现成可用状态，不得为本门禁自行修改，须另行批准或将相关观察
   标为`NOT RUN`。

因此M9源码/主机/交叉构建可以报告PASS，但“开机启动和异常拉起”总退出门禁仍为
`NOT MET`。M10没有开始。

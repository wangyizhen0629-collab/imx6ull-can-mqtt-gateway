# M10 Windows仓库预检报告

结论：**PASS（仅清单第1节）；M10仍为NOT MET**。

## 仓库预检

- `git fetch origin`与`git pull --ff-only origin master`均exit 0，pull结果为`Already up to date.`。
- `HEAD`与`origin/master`均精确为
  `6ee5d475f5451e6cba72f0041613009ed9fc9250`。
- `691c3bd`是当前`HEAD`祖先，exit 0。
- 预检时已跟踪工作树为空；大量既有未跟踪历史目录以及本次新artifact均未被删除、覆盖或
  纳入该“已跟踪工作树干净”结论。
- 当前文档状态复核为M9 `MET`、M10 `NOT MET`。

## 分析器复核

- `test_extended_target_id_is_rejected`存在。
- 精确执行`python tools/protocol/test_analyze_m10_candump.py -v`：8/8 PASS，exit 0；扩展帧
  文本ID边界用例PASS。
- 首次PowerShell 5.1捕获把unittest写到stderr的空行显示为两条
  `System.Management.Automation.RemoteException`包装文本，但测试exit 0、8/8和`OK`均完整。
  为消除歧义，使用`cmd.exe`合并stdout/stderr后追加复跑，输出干净且仍为8/8 PASS、exit 0；
  原始首次记录未改写。

## 本机工具记录

- Windows：`Microsoft Windows NT 10.0.26200.0`；PowerShell：`5.1.26100.9168`；
  Git：`2.51.0.windows.1`；Python：`3.11.7`。
- Keil uVision：`5.30.0.0`；ARM Compiler：`5.06 update 6 (build 750)`；
  STM32CubeMX文件版本：`>6.17.0-RC5`。
- Keil目录中的ST-Link升级工具文件版本：`2.5.2`；ST-Link USB驱动DLL文件版本：`5.1.2.0`。
- Mosquitto broker自报`2.1.2`；`mosquitto_pub`自报`2.1.2`并链接`libmosquitto 2.1.0`；
  OpenSSH产品版本为`OpenSSH_9.5p2 for Windows`。
- `mosquitto_pub --help`打印版本和帮助后返回1。首个补充脚本错误地要求其返回0，因而保留
  `supplement_exit=1`；纠正判定验证其预期exit 1及版本文本，最终PASS。版本查询没有提供
  host/topic/message，未尝试连接Broker。查询后只读观察到一个本地`mosquitto`进程；不推断其
  来源，本run未对其执行任何操作。

## 边界与门禁

没有烧录STM32、转交或部署ARM binary、登录目标板、修改CAN/网络/`/etc`、启停目标进程或
Broker，也没有启动短硬件预演及任何长时间测试。Ubuntu报告的正式ARM binary SHA256为
`d234f2c5f0cc732fd56bc43cc2b8f59491944111b430409ca0ab5b6bb07e4fbf`，但本run没有取得该文件，
因此不把该值描述为Windows或板端实测。

清单2.2的binary私有转交/板端非系统目录核验及2.3的运行边界、容量和回收方案仍未关闭；
所有真实硬件场景保持`NOT RUN`。本run到此停止，M10总门禁保持`NOT MET`。


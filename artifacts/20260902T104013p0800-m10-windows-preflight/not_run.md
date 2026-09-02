# NOT RUN

本run只完成M10清单第1节的Windows仓库预检和分析器回归。以下项目均未获本轮授权，
因此保持`NOT RUN`：

- Ubuntu冻结的ARMv7 `RelWithDebInfo` binary向Windows私有传输、Windows端重新计算SHA256、
  非系统目录板端暂存以及实际部署；所需条件是取得精确binary并单独批准私有传输和板端操作。
- 板端`libmosquitto.so.1`实际文件SHA256、ABI、解释器、`NEEDED`和运行时库映射复核；
  所需条件是批准只读板端登录和binary预检。
- STM32 111/500/1000帧/s profile烧录及短candump预演；所需条件是逐项批准Keil/ST-Link
  烧录和硬件短测。
- 受控停止M9 child、启动/停止M10手工gateway进程、CAN状态或配置操作；所需条件是精确目标、
  恢复方案和单独授权。
- 启动、停止或中断本次专用Broker及subscriber；所需条件是冻结端口/配置/恢复方案并单独授权。
- 500帧/s至少30分钟、1000帧/s至少30分钟、20轮每轮至少5分钟Broker中断、111帧/s至少24小时；
  所需条件是先关闭清单2.2/2.3缺口、完成通用硬件预演并取得长时间测试授权。

因此本run不产生binary部署、物理CAN速率、端到端吞吐、CPU/RSS、断网恢复或长期可靠性结论，
M10总门禁保持`NOT MET`。


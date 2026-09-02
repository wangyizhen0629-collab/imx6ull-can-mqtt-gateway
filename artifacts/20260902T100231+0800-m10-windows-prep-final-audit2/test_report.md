# M10 Windows准备最终审计

结论：**PASS（仅Windows准备提交）；M10仍为NOT MET**。

## PASS

- M10 candump分析器单元测试：7/7，exit 0。
- Keil target generator一致性：PASS，exit 0。
- Python源码、profile JSON和Keil XML解析：PASS，exit 0。
- 新分析器重放既有M4真实物理抓包：6660帧，观测59.988758秒、111.004132帧/s；
  `0x100/0x101/0x102`为6000/600/60，counter gap与CAN error增量均为0，PASS。
- Keil M10_500构建依赖审计：535条依赖记录、51个唯一仓库输入；除本次预定新增的
  `ecu_traffic_profile.h`外全部已由Git跟踪，意外未跟踪依赖为0。
- 三个Keil构建目录均被Git忽略，已跟踪构建产品数均为0。
- 前三个本次Windows artifact manifest逐文件复核PASS。
- `git diff --check` exit 0；预定提交范围敏感扫描PASS。

Windows没有安装CMake/CTest，所以包含新增测试注册后的全量CTest为**NOT RUN**；必须由
Ubuntu拉取本提交后执行warning-clean全量CTest、分析器回归和`RelWithDebInfo` ARM构建、
ELF/依赖/RPATH/SHA复核。Ubuntu明确放行前不得烧录或启动硬件短测。

## 保留失败历史

`20260902T095951+0800-m10-windows-prep-final-audit`在识别预定新增头文件时因PowerShell 5.1
stderr终止语义而INCOMPLETE，已独立归档且manifest PASS；本run未覆盖它。

## NOT RUN

- Ubuntu复核和正式`RelWithDebInfo` ARM binary：NOT RUN。
- STM32烧录、111/500/1000三档短candump、120秒板端指标预演、短端到端对账：NOT RUN。
- 500帧/s与1000帧/s各至少30分钟：NOT RUN。
- 20轮Broker中断：NOT RUN。
- 111帧/s至少24小时：NOT RUN。

没有修改目标板、CAN、Broker、网络或进程，也没有产生性能、CPU/RSS或长期可靠性结论。

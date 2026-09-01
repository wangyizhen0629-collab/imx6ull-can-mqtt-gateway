# M9主机warning-clean与全量回归

- 最终结果：`PASS`
- 首次配置：`FAIL/exit 1`，默认路径缺少libmosquitto开发文件
- 修正配置：`PASS/exit 0`，显式使用前阶段x86_64私有依赖
- 构建：`PASS/exit 0`，warning scan无命中
- 受限沙箱CTest：`FAIL/16 of 18`；PF_CAN和TCP socket权限导致两个既有单测失败，M9
  专项已PASS
- 相同构建沙箱外CTest：`PASS/18 of 18`，M9专项`1/1`

所有尝试使用不同文件名保留，失败输出没有覆盖。该run证明Ubuntu主机构建和回归，
不证明ARM或目标板init行为。

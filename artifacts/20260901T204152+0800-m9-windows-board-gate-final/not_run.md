# 本轮NOT RUN

- 真实reboot后的新boot ID与开机自动启动：目标SSH不可达；需要恢复同一目标的只读登录。
- post-boot唯一supervisor/child、PID文件、exe/SHA/PPID、动态库映射和稳定性：同上。
- post-boot CAN/Broker只读状态及最终无测试进程：同上。
- rollback：reboot前没有触发条件；reboot后若需要，因登录不可用无法执行。需要先恢复
  目标登录，再只读判断当前状态并决定是否运行已保存的rollback脚本。
- Broker交付：reboot前仅见SYN-SENT；没有授权也没有尝试修改Broker外部条件。
- 正确UTC、性能、压力、长时、24小时、CPU/RSS、时延/吞吐/可靠性：不属于M9授权。
- M10：禁止且未开始。

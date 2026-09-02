# NOT RUN

- LeakSanitizer：NOT RUN；本轮sanitizer使用`ASAN_OPTIONS=detect_leaks=0`，只运行ASan和UBSan。
- 真实i.MX6ULL部署、执行、掉电和块设备flush验证：NOT RUN；本轮禁止板端操作。
- STM32烧录、三档真实candump、物理CAN：NOT RUN；本轮禁止。
- 120秒板端指标/短端到端预演：NOT RUN；本轮禁止板端、CAN和Broker测试。
- 500/1000帧/s各30分钟：NOT RUN；本轮禁止长时间和真实硬件测试。
- 20轮Broker中断：NOT RUN；本轮禁止Broker控制和长时间测试。
- 111帧/s 24小时：NOT RUN；本轮禁止长时间测试。

因此不得形成在线写放大、CPU/RSS、吞吐、时延、掉电或长期可靠性结论，M10保持
`NOT MET`。


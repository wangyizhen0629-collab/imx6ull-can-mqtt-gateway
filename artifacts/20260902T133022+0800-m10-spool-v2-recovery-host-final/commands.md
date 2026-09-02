# 命令记录

```sh
cmake -S . -B build/20260902T133022-m10-spool-v2-recovery-host \
  -DCMAKE_BUILD_TYPE=Debug \
  -DGATEWAY_MOSQUITTO_INCLUDE_DIR=/tmp/imx6ull-m6-mosquitto-root/usr/include \
  -DGATEWAY_MOSQUITTO_LIBRARY=/tmp/imx6ull-m6-mosquitto-root/usr/lib/x86_64-linux-gnu/libmosquitto.so
cmake --build build/20260902T133022-m10-spool-v2-recovery-host \
  --clean-first --verbose --parallel
ctest --test-dir build/20260902T133022-m10-spool-v2-recovery-host --output-on-failure
ctest --test-dir build/20260902T133022-m10-spool-v2-recovery-host --output-on-failure -L M7
ctest --test-dir build/20260902T133022-m10-spool-v2-recovery-host --output-on-failure -L M8
ctest --test-dir build/20260902T133022-m10-spool-v2-recovery-host --output-on-failure -L M9
ctest --test-dir build/20260902T133022-m10-spool-v2-recovery-host --output-on-failure -L M10
```

每条命令的原始输出和退出码保存在同目录日志；全部最终命令exit 0。

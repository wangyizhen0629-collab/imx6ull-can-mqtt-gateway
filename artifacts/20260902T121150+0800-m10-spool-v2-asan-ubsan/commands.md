# 执行命令

```sh
cmake -S . -B build/20260902T121150-m10-spool-v2-asan \
  -DCMAKE_BUILD_TYPE=Debug -DGATEWAY_ENABLE_SANITIZERS=ON \
  -DGATEWAY_MOSQUITTO_INCLUDE_DIR=/tmp/imx6ull-m6-mosquitto-root/usr/include \
  -DGATEWAY_MOSQUITTO_LIBRARY=/tmp/imx6ull-m6-mosquitto-root/usr/lib/x86_64-linux-gnu/libmosquitto.so
cmake --build build/20260902T121150-m10-spool-v2-asan --parallel
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build/20260902T121150-m10-spool-v2-asan --output-on-failure
```

命令通过`script -q -e`保存；日志末尾记录真实退出码。

# 执行命令

下列实质命令通过`script -q -e`分别保存完整输出；日志末尾记录真实退出码：

```sh
cmake -S . -B build/20260902T121149-m10-spool-v2-host \
  -DCMAKE_BUILD_TYPE=Debug \
  -DGATEWAY_MOSQUITTO_INCLUDE_DIR=/tmp/imx6ull-m6-mosquitto-root/usr/include \
  -DGATEWAY_MOSQUITTO_LIBRARY=/tmp/imx6ull-m6-mosquitto-root/usr/lib/x86_64-linux-gnu/libmosquitto.so
cmake --build build/20260902T121149-m10-spool-v2-host --clean-first --verbose --parallel
ctest --test-dir build/20260902T121149-m10-spool-v2-host --output-on-failure
ctest --test-dir build/20260902T121149-m10-spool-v2-host -L M7 --output-on-failure
ctest --test-dir build/20260902T121149-m10-spool-v2-host -L M8 --output-on-failure
ctest --test-dir build/20260902T121149-m10-spool-v2-host -L M9 --output-on-failure
ctest --test-dir build/20260902T121149-m10-spool-v2-host -L M10 --output-on-failure
```

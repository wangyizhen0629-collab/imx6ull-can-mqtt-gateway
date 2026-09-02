# 命令记录

第一次configure使用同一toolchain参数但遗漏`IMX6ULL_SDK_ROOT`，exit 1。纠正后的命令：

```sh
IMX6ULL_SDK_ROOT=/home/wangyizhen/projects/imx6ull-can-mqtt-gateway/ToolChain/arm-buildroot-linux-gnueabihf_sdk-buildroot \
cmake -S . -B build/20260902T133024-m10-spool-v2-recovery-arm-relwithdebinfo-v2 \
  -DCMAKE_TOOLCHAIN_FILE=gateway/cmake/toolchains/imx6ull-buildroot.cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=OFF -DCMAKE_SKIP_RPATH=TRUE \
  -DGATEWAY_MOSQUITTO_INCLUDE_DIR=build/m6-arm-private/stage/usr/include \
  -DGATEWAY_MOSQUITTO_LIBRARY=build/m6-arm-private/stage/usr/lib/libmosquitto.so.2.0.11
IMX6ULL_SDK_ROOT=/home/wangyizhen/projects/imx6ull-can-mqtt-gateway/ToolChain/arm-buildroot-linux-gnueabihf_sdk-buildroot \
cmake --build build/20260902T133024-m10-spool-v2-recovery-arm-relwithdebinfo-v2 \
  --clean-first --verbose --parallel
```

之后使用SDK的`file`、`readelf`和`sha256sum`检查产物；完整输出/退出码在同目录日志。

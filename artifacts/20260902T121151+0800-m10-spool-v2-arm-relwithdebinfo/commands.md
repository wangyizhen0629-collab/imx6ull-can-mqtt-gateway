# 执行命令

```sh
cmake -S . -B build/20260902T121151-m10-spool-v2-arm-relwithdebinfo \
  -DCMAKE_TOOLCHAIN_FILE=gateway/cmake/toolchains/imx6ull-buildroot.cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=OFF -DCMAKE_SKIP_RPATH=TRUE \
  -DGATEWAY_MOSQUITTO_INCLUDE_DIR=build/m6-arm-private/stage/usr/include \
  -DGATEWAY_MOSQUITTO_LIBRARY=build/m6-arm-private/stage/usr/lib/libmosquitto.so.2.0.11
cmake --build build/20260902T121151-m10-spool-v2-arm-relwithdebinfo --parallel
sha256sum build/20260902T121151-m10-spool-v2-arm-relwithdebinfo/gateway/gatewayd
cmake --build build/20260902T121151-m10-spool-v2-arm-relwithdebinfo --clean-first --verbose --parallel
sha256sum build/20260902T121151-m10-spool-v2-arm-relwithdebinfo/gateway/gatewayd
file build/20260902T121151-m10-spool-v2-arm-relwithdebinfo/gateway/gatewayd
arm-buildroot-linux-gnueabihf-readelf -h build/20260902T121151-m10-spool-v2-arm-relwithdebinfo/gateway/gatewayd
arm-buildroot-linux-gnueabihf-readelf -l build/20260902T121151-m10-spool-v2-arm-relwithdebinfo/gateway/gatewayd
arm-buildroot-linux-gnueabihf-readelf -d build/20260902T121151-m10-spool-v2-arm-relwithdebinfo/gateway/gatewayd
```

实际工具使用仓库`ToolChain/.../bin/`下的绝对路径；命令通过`script -q -e`保存，日志末尾
记录真实退出码。

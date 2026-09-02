# M10 Ubuntu复核命令

工作目录：`/home/wangyizhen/projects/imx6ull-can-mqtt-gateway`

以下命令用于本run；输出分别保存为同目录对应日志。涉及网络的`git pull`在获得工具权限
后执行；没有访问目标板、CAN、Broker或Windows主机。

```sh
git status --short --branch
git pull --ff-only origin master
git rev-parse HEAD
git rev-parse origin/master
git diff --stat f67b3652e382dc41d97750b74df71b5d6d1d88cb..b25cab851c2daf8e7b19d6eb3338747d400d06c8

# Windows清单的直接尝试及CRLF兼容只读复核
(cd <windows-artifact> && sha256sum -c artifact_manifest.sha256)
(cd <windows-artifact> && tr -d '\r' < artifact_manifest.sha256 | sha256sum -c -)

# Ubuntu Debug warning-clean构建和测试
cmake -S . -B build/m10-ubuntu-review-host-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/m10-ubuntu-review-host-debug --parallel
ctest --test-dir build/m10-ubuntu-review-host-debug --output-on-failure
python3 tools/protocol/test_analyze_m10_candump.py -v
python3 tools/stm32/generate_keil_targets.py --check

# 已归档M4真实6660帧candump重放
python3 tools/protocol/analyze_m10_candump.py \
  --candump artifacts/20260830T215631+0800-m4-can-model-final/candump_60s.log \
  --profile 111 --minimum-duration-seconds 59 \
  --can-before artifacts/20260830T215631+0800-m4-can-model-final/can_stats_before.txt \
  --can-after artifacts/20260830T215631+0800-m4-can-model-final/can_stats_after.txt \
  --output artifacts/20260902T101743+0800-m10-ubuntu-review/m4_replay_summary_v2.json

# 主机RelWithDebInfo诊断构建；真实FAIL保存在host_build.log
cmake -S . -B build/m10-ubuntu-review-host -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/m10-ubuntu-review-host --parallel

# 正式ARMv7 RelWithDebInfo输入
cmake -S . -B build/m10-ubuntu-review-arm-relwithdebinfo \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=OFF \
  -DCMAKE_SKIP_RPATH=TRUE \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-buildroot-linux-gnueabihf.cmake \
  -DMOSQUITTO_INCLUDE_DIR=build/m6-arm-private/stage/usr/include \
  -DMOSQUITTO_LIBRARY=build/m6-arm-private/stage/usr/lib/libmosquitto.so.2.0.11
cmake --build build/m10-ubuntu-review-arm-relwithdebinfo --parallel
cmake --build build/m10-ubuntu-review-arm-relwithdebinfo --clean-first --verbose --parallel
sha256sum build/m10-ubuntu-review-arm-relwithdebinfo/gateway/gatewayd
file build/m10-ubuntu-review-arm-relwithdebinfo/gateway/gatewayd
arm-buildroot-linux-gnueabihf-readelf -h build/m10-ubuntu-review-arm-relwithdebinfo/gateway/gatewayd
arm-buildroot-linux-gnueabihf-readelf -l build/m10-ubuntu-review-arm-relwithdebinfo/gateway/gatewayd
arm-buildroot-linux-gnueabihf-readelf -d build/m10-ubuntu-review-arm-relwithdebinfo/gateway/gatewayd
```

沙箱内CTest因PF_CAN和保留socket权限为19/21；按测试要求在沙箱外重跑，最终当前源码
21/21 PASS。ARM binary只在忽略的`build/`中生成，没有部署或运行。

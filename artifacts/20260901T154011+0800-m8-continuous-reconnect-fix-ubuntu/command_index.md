# 命令与证据索引

除 `initial_pull.log` 是从第一次安全检查和fast-forward pull的真实终端输出原样转录外，
其余命令日志由 `script -q -e -c` 直接捕获，文件头尾包含执行时间和真实退出码，正文中
的 `set -x` 行保存实际命令。

## Preflight与环境

- `initial_pull.log`：首次`git status --short --branch`和
  `git pull --ff-only origin master`。
- `git-head.log`、`git-ancestor.log`、`git-show-stat.log`、
  `git-status-after-pull.log`：提交身份、祖先关系、修复stat和pull后状态。
- `uname.log`、`os-release.log`、`cc-version.log`、`cmake-version.log`、
  `make-version.log`：主机与工具版本。
- `libmosquitto-version.log`、`libmosquitto-dpkg-version.log`：系统包元数据查询的真实
  失败；没有因此安装依赖。
- `host-libmosquitto-audit.log`、`target-libmosquitto-audit.log`：实际使用的主机/目标
  头文件、库路径、版本宏、链接解析和SHA256。
- `fix-source-audit.log`、`source-sha256.log`、`fresh-build-dir-check.log`：修复测试存在、
  当前源码哈希和三个构建目录初始不存在。

## 普通主机构建与测试

- `host-configure.log`：fresh Debug、`BUILD_TESTING=ON`、显式主机libmosquitto开发集。
- `host-build.log`：warning-clean完整构建输出。
- `host-test-mqtt-sink-direct.log`、`host-ctest-m8.log`、
  `host-ctest-full-sandboxed.log`：受限沙箱首次失败，原样保留。
- `host-test-mqtt-sink-direct-unsandboxed.log`、`host-ctest-m8-unsandboxed.log`、
  `host-ctest-full-unsandboxed.log`：同一fresh binary的沙箱外最终结果。
- `host-fresh-binary-audit.log`：源码/测试binary时间、大小和SHA256。

## ASan+UBSan

- `asan-ubsan-configure.log`、`asan-ubsan-build.log`：fresh sanitizer configure/build。
- `asan-ubsan-test-mqtt-sink-direct.log`、`asan-ubsan-test-mqtt-reactor-direct.log`：直接测试。
- `asan-ubsan-ctest-m8.log`：带明确ASAN/UBSAN环境变量的M8 CTest。

## ARM交叉构建与ELF

- `arm-toolchain-version.log`、`arm-configure.log`、`arm-build.log`：既有SDK版本、fresh
  configure和warning-clean构建。
- `arm-binary-identity.log`、`arm-file.log`：新binary路径、大小、SHA256和file输出。
- `arm-readelf-header.log`、`arm-readelf-program-headers.log`、
  `arm-readelf-dynamic.log`、`arm-readelf-relocations.log`：要求的`readelf -hW/-lW/-dW/-rW`。
- `arm-rpath-audit.log`：RPATH/RUNPATH计数0。
- `arm-external-api-audit.log`：5个API动态重定位行。
- `arm-external-api-count.log`：首次awk转义失败；保留用于失败审计。
- `arm-external-api-unique-retry.log`、`arm-external-api-unique-count.log`：成功重试，5个
  唯一external-loop API。
- `cmake-cache-audit.log`、`warning-audit.log`：三个fresh配置关键项和三份build合并
  warning计数。

## 提交审计

- `git-diff-check-before-stage.log`、`git-status-before-stage.log`：暂存前检查。
- `sensitive-scan.log`：私钥、凭据和IPv4模式匹配计数0。
- `staged-name-status.log`、`staged-stat.log`：只暂存本run证据时的文件清单与stat。
- `git-diff-cached-check.log`：完整cached check；PTY CRLF导致trailing whitespace报告，
  退出2，原始日志没有改写。
- `git-diff-cached-check-markdown.log`：人工维护Markdown的cached check退出0。
- `manifest.sha256`：不包含自身，覆盖本目录其余最终证据文件。

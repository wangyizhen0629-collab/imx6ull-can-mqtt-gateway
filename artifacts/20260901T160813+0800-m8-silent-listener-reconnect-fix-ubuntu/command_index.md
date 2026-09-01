# 命令与证据索引

除`initial_pull.log`为首次status/pull真实输出的原样转录外，其余命令日志由
`script -q -e -c`直接捕获；文件头尾记录执行时间和退出码，`set -x`记录实际命令。

## Preflight与焦点源码

- `initial_pull.log`：安全status与`git pull --ff-only origin master`。
- `git-head.log`、`git-ancestor.log`、`git-show-stat.log`、
  `git-status-after-pull.log`：目标提交身份与状态。
- `focused-test-source.log`、`focused-test-no-explicit-poll.log`、
  `focused-test-no-explicit-poll-retry.log`：完整焦点测试和显式poll调用审计；首次计数
  输出为空字符串但退出0，重试明确输出0，两份均保留。
- `silent-listener-source.log`、`silent-listener-no-response-audit.log`：listener实现和
  accept/send/write调用计数0。
- `focused-requirements-audit.log`、`source-sha256.log`：五项关键断言/异步重连源码位置
  与源码哈希。
- `fresh-build-dir-check.log`：三个指定build目录开始前均不存在。
- `host-tool-versions.log`、`host-libmosquitto-audit.log`、
  `target-libmosquitto-audit.log`：工具和依赖身份。

## 普通主机测试

- `host-configure.log`、`host-build.log`：fresh warning-clean构建。
- `host-test-mqtt-sink-direct.log`：直接执行焦点binary。
- `host-test-mqtt-sink-repeat10.log`：焦点CTest连续重复10次。
- `host-ctest-m8.log`：M8正则专项。
- `host-ctest-full.log`：全量CTest。
- `host-fresh-binary-audit.log`：测试binary与源码时间、大小、SHA256。

## Sanitizer

- `asan-ubsan-configure.log`、`asan-ubsan-build.log`：fresh ASan+UBSan构建。
- `asan-ubsan-test-mqtt-sink-direct.log`：sanitizer焦点直接测试。
- `asan-ubsan-ctest-m8.log`：带明确ASAN/UBSAN环境变量的M8专项。

## ARMv7

- `arm-toolchain-version.log`、`arm-configure.log`、`arm-build.log`：既有SDK版本、fresh
  configure和warning-clean构建。
- `arm-binary-identity.log`、`arm-file.log`：路径、大小、SHA256和file结果。
- `arm-readelf-header.log`、`arm-readelf-program-headers.log`、
  `arm-readelf-dynamic.log`、`arm-readelf-relocations.log`：完整ELF审计。
- `arm-mqtt-api-audit.log`、`arm-mqtt-api-unique.log`、`arm-mqtt-api-count.log`：五个
  external-loop API及`mosquitto_connect_async`的动态引用和唯一计数6。
- `arm-rpath-audit.log`：RPATH/RUNPATH计数0。
- `warning-audit.log`、`cmake-cache-audit.log`：warning计数和三个fresh配置关键项。

## 提交前审计

- `git-diff-check-before-stage.log`、`git-status-before-stage.log`：暂存前工作树检查。
- `artifact-file-list-before-stage.log`、`artifact-size.log`：artifact文件清单与大小。
- `secret-scan.log`、`ipv4-scan.log`、`forbidden-artifact-scan.log`：敏感模式、地址和禁止
  构建产物扫描。
- `git-diff-cached-check.log`：保留原始`script`日志CRLF导致的全量空白检查退出2。
- `git-diff-cached-check-markdown.log`：人工维护Markdown专项检查退出0。
- `git-diff-cached-name-status.log`、`git-diff-cached-stat.log`、
  `git-status-after-stage.log`：首次暂存后的范围和状态。
- `final-staged-scope-audit.log`、`final-staged-binary-audit.log`、
  `final-git-status-before-commit.log`：最终暂存范围、二进制排除和提交前状态。
- `manifest.sha256`、`manifest-check.log`：除清单自身和校验输出外的artifact文件SHA256
  清单及校验结果；排除这两个循环依赖文件。

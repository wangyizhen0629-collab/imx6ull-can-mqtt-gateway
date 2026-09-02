# 扫描结果解释

- `sensitive-scan.log`对staged diff搜索常见凭据赋值、私钥头、URL内嵌凭据和IPv4字面量；
  `rg`退出1且无匹配是预期PASS语义，不是命令故障。
- `tracked-binary-audit.log`检查staged文件名及numstat中的二进制标记；无构建产物或binary
  条目为PASS。
- 文档中的SHA256、本机build目录、动态链接器路径和公开配置键不是凭据，也不包含真实
  局域网地址。


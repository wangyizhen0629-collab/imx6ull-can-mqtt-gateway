# 扫描结果解释

- `sensitive-scan.log`：`rg`真实退出码1，表示对基线新增diff和四个新artifact目录没有匹配
  私钥头、常见云key形态、非空密码/令牌赋值或RFC1918地址；判定PASS。
- `tracked-binary-audit.log`：`rg`真实退出码1，表示候选变更文件列表没有匹配build、
  ToolChain、Objects、Listings、private_raw或二进制/目标文件扩展名；判定PASS。
- `git-diff-check.log`：退出码0，PASS。
- `sensitive-scan-final.log`和`tracked-binary-audit-final.log`：文档收尾后的最终复扫也均为
  `rg`真实退出码1（无匹配），PASS；`git-diff-check-final.log`退出码0，PASS。

最初尝试用嵌套`bash -c`把“无匹配exit 1”转换成0时，外层引号解析在正则的`(`处失败，
两个工具调用均exit 2，审计没有执行。该失败发生在`script`创建日志之前，所以没有原始日志
文件；实际终端错误为`syntax error near unexpected token '('`。随后改为直接保存`rg`完整
输出和真实exit 1，并按上述标准解释，没有覆盖或把exit 1伪装成命令PASS。

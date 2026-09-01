# 最终审计首轮失败记录

`finalize_evidence.ps1`于证据封存前执行并以exit 1停止，所有首轮输出均原样保留，未覆盖。
失败不是新增板端门禁结果：

1. 固定查找`C:\Program Files\Git\bin\bash.exe`，本机没有Git Bash，因此把“解析器不可用”
   计成了shell语法失败；本轮实际执行过的板端脚本由目标BusyBox 1.31.1 ash运行，未执行的
   post-reboot/rollback脚本不能据此声称已完成语法门禁。
2. 文档规则机械要求八个文档都重复完整run ID和M10句式，导致四个只保留摘要/待办语义的
   文档误报；它们仍明确保持M9 `NOT MET`，且仓库总表/M9里程碑明确M10未开始。
3. 凭据正则在Windows CRLF文本上把`broker_username=`和下一行空的
   `broker_password=`合并成非空命中；公开文件中的两项值实际均为空，broker host为
   `<REDACTED>`。

第二版审计将保留shell解析器`NOT RUN`，按文档各自职责检查必需语义，并对凭据逐行解析。
首轮manifest自身复核为PASS，但在加入本说明和第二版审计后不再作为最终manifest使用。

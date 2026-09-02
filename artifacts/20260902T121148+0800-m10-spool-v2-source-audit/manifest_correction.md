# Manifest纠正记录

首次manifest生成后，最终提交前自检发现`test_report.md`声称将写入一个不可能在提交内容中
自引用的最终SHA，现已改为由提交后Git ref和交接报告核对。原manifest/check保留为修改前
历史；`artifact_manifest.v2.sha256`是本目录纠正后的最终清单。

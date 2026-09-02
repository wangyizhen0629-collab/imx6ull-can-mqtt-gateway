# Manifest纠正记录

首次manifest生成后，最终提交前按原始单份clean verbose日志确认使用的是一次
`cmake --build --clean-first --verbose --parallel`调用，故更正`commands.md`，未修改构建、
ELF或SHA原始日志。原manifest/check保留；`artifact_manifest.v2.sha256`为最终清单。

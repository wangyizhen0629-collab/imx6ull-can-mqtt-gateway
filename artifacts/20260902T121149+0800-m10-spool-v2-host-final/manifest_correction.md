# Manifest纠正记录

首次manifest生成后，最终提交前按原始单份build日志确认clean与verbose build来自一次
`cmake --build --clean-first --verbose --parallel`调用，故更正`commands.md`，未修改任何
构建或测试原始日志。原manifest/check保留；`artifact_manifest.v2.sha256`为最终清单。

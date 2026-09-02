# 提交前manifest纠正记录

首次生成manifest后，按三个实际`CMakeCache.txt`复核发现人工`commands.md`中的
libmosquitto CMake变量名和输入路径写得不精确。仅纠正文档为实际
`GATEWAY_MOSQUITTO_INCLUDE_DIR/GATEWAY_MOSQUITTO_LIBRARY`及真实路径；没有重跑、删除或
改写构建/测试原始日志。随后重新生成四个新run的payload/artifact manifest并全部复核。


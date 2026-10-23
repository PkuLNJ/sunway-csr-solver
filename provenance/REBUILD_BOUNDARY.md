# 构建身份与来源边界

n80 主程序 SHA-256 为 `be97dddf6c282ee839ed2dfa5948c7acc59c39d075de799c6dd2ef4df6aa4c82`，来自已完成 Job 8677513 的运行记录。

此次从神威只读读取实际应用目录、组件目录，以及沿历史构建脚本递归发现的 26 个构建目录中的源码。`SOURCE_CAPTURE.json` 保留每个文件的远端路径和原始字节哈希。历史链中的旧组件用于保全来源，不能把每一份旧源码都当成最终链接版本。

主要有效覆盖顺序见 `build/historical/build-n80.sh` 和 `provenance/components/n78/build.sh`：n80 的 `csrSymGaussSeidelSmoother.C` 替换 n78 archive 对应唯一成员；n78 已合入 `CSRGAMGSolverSolve`、`csrMatrixSmoother` 及 combined CPE kernel。头文件按真实 `-I` 顺序取用，不可把多个组件目录平铺覆盖。

`reference-library-source` 是已保存的 of0404 库参考源代码；完整 OpenFOAM、ThirdParty、wmake、系统库仍在原机器环境，Git 没有将预编译库伪装为源码可重建证据。

以下内容已分别保存：

- n80 主版本的二进制、dfMatrices archive、入口对象、native OpenFOAM、插件对象及 seaWave 身份记录；
- 小改动实际编译使用的最终头文件和两份编译命令，而非最初未适配 tmp API 的草稿；
- 50 步实际日志及原始运行配置；候选状态仍为未完成 CFD 验证。

本次只做保全和完整性核验，没有执行新 CFD 算例、重编库、修改生产目录或推送远端 Git 服务。

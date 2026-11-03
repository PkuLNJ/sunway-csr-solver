# 神威 CSR 求解器

同一套源码构建 CSR 主核版（host）与主从核版（cpe）。使用双精度浮点、64位整数索引。

## 目录

- `csr/OpenFOAM-7/`：OpenFOAM、dfMatrices等基础库源码和wmake规则。
- `csr/src/`：求解器、seaWave、CSR/GAMG、SGS、从核内核及项目修改。
- `csr/build-config/compiler-options.mk`：编译器和数值编译选项。
- `csr/build-config/solver.mk`：源文件清单和链接规则。
- `csr/Makefile`：通过BACKEND选择host或cpe。
- `environment.sh`、`modules.txt`、`build-libraries.sh`：基础库构建环境、模块清单和构建脚本。
- `ldu/`：移植曙光原版的LDU对照源码及构建记录。
- `validation/`：本机验证算例和结果，保留在服务器。

## 编译环境

需要神威编译器、MPI、运行库，以及make、flex和m4。当前服务器使用MPI目录 `/usr/sw/mpi/mpi_20220608_SEA`，编译器工具目录 `/usr/sw/swgcc/swgcc710-tools-SEA-1307`。工具路径见 `csr/env.sh` 和构建规则。

## 编译主核与主从核版本

在本目录执行：

```bash
source csr/env.sh host
export PATH=/home/export/online1/mdt00/shisuan/swpku_of/guoshi/tools/flex/bin:$PATH
export M4=/home/export/online1/mdt00/shisuan/swpku_of/guoshi/tools/m4/bin/m4

JOBS=4 bash build-libraries.sh csr > libraries-csr.log 2>&1
# 上一步成功后继续
make -C csr -j4 solver BACKEND=host > solver-host.log 2>&1
make -C csr -j4 solver BACKEND=cpe > solver-cpe.log 2>&1
```

基础库由本目录源码编译；上层修改加入本次生成的对应静态库，再链接求解器。两版使用同一套上层源码和数值编译设置，分别选择主核与从核实现。完整编译命令保存在上述日志中。

产物：

```text
csr/build/host/bin/boussinesqFoam
csr/build/cpe/bin/boussinesqFoam
```

运行前在 `csr/` 内执行 `source env.sh host` 或 `source env.sh cpe`，再通过神威调度系统提交匹配算例。算例、分区数、队列和资源参数按具体运行设置。

## 已完成验证

2026-11-02：基础库和两个程序编译退出码均为0。单节点6MPI、一个时间步验证，两版分别与各自已有程序比较，七字段内部及边界共14组逐值一致。

- 主核新编/参考作业：8906704 / 8906706。
- 主从核新编/参考作业：8906716 / 8906717。
- 结果：`host-runtime-pass.json`、`cpe-runtime-pass.json`。

## GitHub 源码保存

`.gitignore` 排除编译产物、运行数据和过程日志。上传前检查实际暂存清单、源码软链接目标、文件体积以及许可证。OpenFOAM及第三方源码保留各自许可证和版权声明。源码仓库配合上述神威工具链构建；运行算例另行提供。

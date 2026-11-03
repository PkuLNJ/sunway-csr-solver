# 神威求解器构建说明

此目录包含 LDU 对照版及 CSR 主核／主从核版的源码和构建规则。三版均采用双精度浮点、64位整数索引。

## 源码与构建关系

- `csr/OpenFOAM-7/`：OpenFOAM、dfMatrices 等基础库源码、wmake 规则。
- `csr/src/solver/`、`csr/src/seaWave/`：求解器入口、方程代码和波浪边界。
- `csr/src/csr/`、`csr/src/sgs/`、`csr/src/kernels/`：CSR/GAMG 修改、SGS 数据组织及主核／从核内核。
- `csr/src/openfoam/`、`csr/src/fixes/` 等：基础函数修改和辅助实现；完整编译单元见 `csr/Makefile`、`csr/build-config/solver.mk`。
- `csr/build-config/compiler-options.mk`：编译器、精度和优化选项。
- `ldu/OpenFOAM-7/`、`ldu/source/`：LDU 基础库、移植的曙光求解器和 seaWave 源码。
- `environment.sh`、`modules.txt`、`build-libraries.sh`：基础库编译环境、模块顺序和构建脚本。

构建先生成基础静态库，再编译项目修改并更新对应库，最后链接求解器。CSR 两版共用上层源码，通过 `BACKEND=host` 或 `BACKEND=cpe` 选择内核执行方式，产物分目录保存。

## 获取与环境

请在区分大小写的 Linux 文件系统上克隆。以下命令适用于当前神威服务器，需能访问 GitHub 私有仓库：

```bash
git clone https://github.com/PkuLNJ/sunway-csr-solver.git
cd sunway-csr-solver/source-build
```

机器需要提供神威编译器、MPI、运行库、make、flex 和 m4。当前工具路径：

- MPI：`/usr/sw/mpi/mpi_20220608_SEA`
- 编译器：`/usr/sw/swgcc/swgcc710-tools-SEA-1307`
- flex：`/home/export/online1/mdt00/shisuan/swpku_of/guoshi/tools/flex/bin`
- m4：`/home/export/online1/mdt00/shisuan/swpku_of/guoshi/tools/m4/bin/m4`

## CSR 主核／主从核编译

在 `source-build/` 执行。下方 `&&` 确保前一步成功后才继续：

```bash
source csr/env.sh host
export PATH=/home/export/online1/mdt00/shisuan/swpku_of/guoshi/tools/flex/bin:$PATH
export M4=/home/export/online1/mdt00/shisuan/swpku_of/guoshi/tools/m4/bin/m4

JOBS=4 bash build-libraries.sh csr > libraries-csr.log 2>&1 &&
make -C csr -j4 solver BACKEND=host > solver-host.log 2>&1 &&
make -C csr -j4 solver BACKEND=cpe > solver-cpe.log 2>&1
```

基础库完成后，上层代码修改可直接执行相应的 `make -C csr` 命令进行增量编译。日志包含展开后的编译和链接命令。

产物：

```text
csr/build/host/bin/boussinesqFoam
csr/build/cpe/bin/boussinesqFoam
```

## LDU 首次构建

沿用上面的工具环境，在新的源码克隆目录中执行：

```bash
JOBS=4 bash build-libraries.sh ldu > libraries-ldu.log 2>&1 &&
bash ldu/build-solver.sh > solver-ldu.log 2>&1
```

产物为 `ldu/bin/boussinesqFoam-LDU`。该脚本会创建一次性构建标记及输出目录，适用于首次构建。

## 运行环境

CSR 运行前进入 `csr/`，执行 `source env.sh host` 或 `source env.sh cpe`，然后使用对应程序通过神威调度系统提交算例。算例输入、分区数量和调度资源由实际测试确定。

## 已完成的源码重建验证

2026-11-02，在服务器构建目录完成基础库和三个程序编译。CSR 两版构建退出码均为0。

此次运行验证采用单节点6MPI、总计48单元的静态网格，计算一步至 `t=0.001`，小测试的 `nCellsInCoarsestLevel=2`。各新编程序分别与对应已有程序比较，七字段（U、p_rgh、TT、S、alpha.water、p、rhok）内部及边界共14组逐值一致。

| 版本 | 新编程序作业 | 参考程序作业 | 数值记录 |
|---|---:|---:|---|
| LDU | 8904761 | 8904763 | `ldu-runtime-pass.json` |
| CSR 主核 | 8906704 | 8906706 | `host-runtime-pass.json` |
| CSR 主从核 | 8906716 | 8906717 | `cpe-runtime-pass.json` |

这项验证用于检查源码重建后的运行和数值一致性。960分区50步的性能及7680分区动态加密分别由对应大算例实验记录。

构建与完整小测试日志保留在服务器：

```text
/home/export/online1/mdt00/shisuan/swpku_of/lnj/reproduce/source-build-20261102
```

该路径下的 `validation/step1/` 保存 CSR 对照算例；仓库保存上述数值汇总。`SOURCE_MANIFEST.json` 记录服务器导出源码时的内容哈希。OpenFOAM 许可证见 `COPYING`，各源码保留原版权声明。

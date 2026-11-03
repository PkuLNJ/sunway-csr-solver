# 神威求解器

提供 LDU 对照版、CSR 主核版和 CSR 主从核版。使用神威编译器与 MPI，双精度浮点、64位整数索引。

## 目录

```text
csr/
  OpenFOAM-7/       基础库源码及 wmake 编译规则
  src/              求解器、seaWave、CSR/GAMG 和主核／从核实现
  build-config/     编译选项及链接规则
  Makefile          通过 BACKEND=host 或 cpe 选择执行版本
  env.sh            神威工具链和运行环境
ldu/
  OpenFOAM-7/       LDU 基础库源码
  source/           移植的曙光求解器和 seaWave 源码
  build-solver.sh   LDU 求解器构建脚本
build-libraries.sh  按 modules.txt 顺序编译基础库
modules.txt        基础库清单
environment.sh     OpenFOAM 编译环境
Makefile           顶层构建入口
```

CSR 两版共用源码，编译时选择主核或从核实现。构建先生成基础库，再编译项目修改、更新对应库并链接求解器。

## 获取源码

请在区分大小写的 Linux 文件系统上克隆；OpenFOAM 中存在仅大小写不同的文件名。

```bash
git clone https://github.com/PkuLNJ/sunway-csr-solver.git
cd sunway-csr-solver
```

## 编译环境

当前神威服务器提供以下工具：

- MPI：`/usr/sw/mpi/mpi_20220608_SEA`
- 编译器：`/usr/sw/swgcc/swgcc710-tools-SEA-1307`
- flex：`/home/export/online1/mdt00/shisuan/swpku_of/guoshi/tools/flex/bin`
- m4：`/home/export/online1/mdt00/shisuan/swpku_of/guoshi/tools/m4/bin/m4`

在仓库根目录配置环境：

```bash
source csr/env.sh host
export PATH=/home/export/online1/mdt00/shisuan/swpku_of/guoshi/tools/flex/bin:$PATH
export M4=/home/export/online1/mdt00/shisuan/swpku_of/guoshi/tools/m4/bin/m4
```

## 编译 CSR 主核与主从核版

```bash
JOBS=4 bash build-libraries.sh csr > libraries-csr.log 2>&1 &&
make -C csr -j4 solver BACKEND=host > solver-host.log 2>&1 &&
make -C csr -j4 solver BACKEND=cpe > solver-cpe.log 2>&1
```

基础库构建完成后，修改上层代码可直接执行相应 `make -C csr` 命令进行增量编译。编译和链接命令保存在上述日志中。

产物：

```text
csr/build/host/bin/boussinesqFoam
csr/build/cpe/bin/boussinesqFoam
```

编译选项见 `csr/build-config/compiler-options.mk`，源文件和链接规则见 `csr/Makefile`、`csr/build-config/solver.mk`。

## 首次编译 LDU 对照版

配置好上述环境后，在新的源码克隆目录执行：

```bash
JOBS=4 bash build-libraries.sh ldu > libraries-ldu.log 2>&1 &&
bash ldu/build-solver.sh > solver-ldu.log 2>&1
```

产物为 `ldu/bin/boussinesqFoam-LDU`。该脚本创建一次性构建标记和输出目录，适用于首次构建。

## 运行与验证

CSR 运行前进入 `csr/`，执行 `source env.sh host` 或 `source env.sh cpe`，再通过神威调度系统提交对应程序。算例输入另行提供。

2026-11-02，三版已在服务器完成源码编译，并分别与对应已有程序完成单节点6MPI、静态小网格一步验证：七个场的内部和边界逐值一致。该验证采用48单元、终止时刻0.001及粗层阈值2，用于确认源码重建后的数值一致性。

OpenFOAM 许可证见 `COPYING`，各源码保留原版权声明。

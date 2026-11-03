# 神威求解器源码

当前完整构建源码位于 **[source-build/](source-build/)**，编译方法见 **[构建说明](source-build/README.md)**。

提供三个程序：

| 版本 | 源码 | 产物（相对 source-build/） |
|---|---|---|
| LDU 对照版 | `ldu/OpenFOAM-7/`、`ldu/source/` | `ldu/bin/boussinesqFoam-LDU` |
| CSR 主核版 | `csr/OpenFOAM-7/`、`csr/src/` | `csr/build/host/bin/boussinesqFoam` |
| CSR 主从核版 | 与主核版共用源码，编译时选择从核实现 | `csr/build/cpe/bin/boussinesqFoam` |

使用神威编译器和 MPI，双精度浮点、64位整数索引。OpenFOAM 基础库由随仓库提供的源码构建。

2026-11-02 完成基础库和三个程序的构建；各版本分别与对应已有程序完成单节点6MPI、静态小网格一步对照，七字段内部和边界数值逐值一致。详见构建说明中的验证记录。

请在神威或其他区分大小写的 Linux 文件系统上克隆。OpenFOAM 中存在仅大小写不同的文件名，源码目录需要保留这种区别。

```bash
git clone https://github.com/PkuLNJ/sunway-csr-solver.git
cd sunway-csr-solver/source-build
```

运行需要另行提供算例，并通过神威调度系统分配资源。仓库中的旧版 `source/`、`build/`、`records/` 等目录保留项目历史；当前构建入口统一使用 `source-build/`。

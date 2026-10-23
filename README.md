# CSR 求解器版本保全

保全日期：2026-10-23。此仓库保存当前 CSR 开发主线，未纳入 bai 大写 B 路线和压力分叉诊断程序。

## 版本

| Git 标识 | 来源 | 状态 |
| --- | --- | --- |
| `csr-main-v1` / `main` | 原实验编号 n80 | 已有 960 MPI、50 步记录；ClockTime 2541 s，Loop 累计 2379.457611 s |
| `experiment/alpha-snapshot` | 主版本 + 删除无用 alpha 快照 | 已完成神威编译/链接，算例数值及收益未验证 |
| `experiment/pressure-div` | 主版本 + 复用非正交循环内不变的散度 | 已完成神威编译/链接，算例数值及收益未验证 |

当前没有两个改动的合并版本。实验分支的状态不因 Git 提交或标签而成为验证通过。

当前应用仅将主压力方程 p_rgh 交给 CSR。U、TT、S 使用原生 LDU 求解。

## 保存范围

- `source/application/`：从实际神威私有构建目录读取的应用入口及 VoF 源码。
- `source/components/`：n80 构建引用的自定义组件源码，保留组件边界，避免同名头文件相互覆盖。
- `reference-library-source/`：既有 of0404 源码快照，供追踪依赖。它不是所有预编译库的位级来源证明。
- `build/`：真实历史构建脚本，包含原始编译选项、库选择和外部路径。
- `config/`：已留存的主版本运行字典和参考配置，来源分别标记。
- `provenance/`：读取清单、原路径、SHA-256、构建产物身份和验证记录。
- `records/`：主版本已完成运行和候选尚未完成验证的证据。
- `VERSION.json`：当前分支的应用/二进制身份及验证状态。

不保存 platforms、静态库、可执行文件、网格、场数据和凭据。大型二进制保留其服务器路径和哈希。

## 重建边界

这是一份源码及构建身份保全，依赖原神威编译器、MPI、ThirdParty 和外部 OpenFOAM 库，不能在普通 Windows/x86 上直接构建。

n80 历史构建复用了入口对象及多份私有 archive；源码现存状态、对象哈希与实际运行二进制身份分别记录。尚未用本仓库做全量重编译，不承诺位一致重建。`build/historical/` 脚本保留原有写入路径和一次性 guard，应先审阅并在全新私有构建目录适配，不能直接在原生产目录重跑。

`provenance/SOURCE_CAPTURE.json` 记录远端原始字节的哈希；Git 将文本行尾标准化，因此它与 Git blob 的哈希不是同一种口径。用 `python tools/verify_snapshot.py` 检查仓库中的标准化源码清单。

## 使用

```bash
git log --oneline --all --decorate
git switch main
git switch experiment/alpha-snapshot
git switch experiment/pressure-div
git diff csr-main-v1..experiment/alpha-snapshot -- source/application
git diff csr-main-v1..experiment/pressure-div -- source/application
python tools/verify_snapshot.py
```

三个分支的实际应用差异应各自只有预期头文件。候选对照数值通过且有可比耗时收益后，才考虑合并。


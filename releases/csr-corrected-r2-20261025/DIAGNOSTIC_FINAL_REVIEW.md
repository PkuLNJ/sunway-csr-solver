# LDU 与 CSR 首步分叉：r2 修复与全场审核

_2026-10-25，北京时间；软件诊断和固定算例第一步的数值审核，不是性能测试或模型物理验证。_

---

## 📋 审核结论

已经定位并私有修复三处实质不一致：波浪入口平均流的处理、实际 SIMD 编译产物中的向量平方函数、初始化 TT/S 后缺少边界同步。最终 r2 的全 960 分区比较确认：初始与 AMR 后五个标量的内部及全部边界逐值相同；首个启动和加密后压力校正方程的公共 A、b、初值与边界系数相同；CSR 非对角系数与同一 LDU 矩阵的全部展开条目严格相同。

**不是全部终场逐值相同。** 原版保留 DIC，CSR 保留 GAMG 及其既有 `minIter=1` 设置，后续解和通量存在可量化差异；顶部有真实的入流/出流分支切换。不能因内部场整体接近就删除该边界差异，也不能把本报告当作所有工况或后续时间步的正确性保证。

主要证据：[根独立复核](C:/Users/15225/Documents/Codex/2026-08-31/n/work/amr-firstdiv-20261023/R2_ROOT_SCIENTIFIC_REVIEW-20261025-01.json)、[全局比较原始汇总](C:/Users/15225/Documents/Codex/2026-08-31/n/work/amr-firstdiv-20261023/io/r2-unified-20261024-01/final-results/aggregate.paired.json)。历史取证过程保留于[原诊断记录](C:/Users/15225/Documents/Codex/2026-08-31/n/work/amr-firstdiv-20261023/DIAGNOSIS.md)。

## 🔧 三项修复及证据边界

| 项目 | 确认的问题 | 私有修复 |
|---|---|---|
| 波浪入口 | 平均流是否乘湿润比例/爬升因子及平均流 Doppler 项不同 | 私有波浪库采用原版实际模型对象 |
| 向量平方 | 实际 `symmTensorField.o` 中 Field `sqr(vector)` 计算错误 | 链接已测正确的强符号替代对象 |
| TT/S 初始化 | 内部值改写后接口缓存未同步 | 在原初始化分支内补两次边界刷新 |

平方函数的具体反例：对 `n=(0,1,0)`，对称张量的六分量应为 `[0,0,0,1,0,0]`，旧 CSR 实际对象返回 `[0,0,0,1,1,0]`。这会把顶部法向投影变成错误的分量混合，同时影响底部 slip。实际提供者是 `libOpenFOAM.a(symmTensorField.o)`，不是 `vectorField.o`。已完成实际对象与 ELF 的代码绑定、目标机数学测试和消融；未成功完成的新鲜四组编译选项实验不能被当作证据，因此不声称已证明某单一编译选项在所有程序中必然产生该错误。

最终程序选中的修复函数为 196 字节，机器码 SHA256 为 `1200b3736cd8055084790c66a2d7ecaf811d2546a84ebed74aeba6c2470079d5`，与经过数学测试的对象逐字节相同。并未修改共享 OpenFOAM 库，因此也不声称其他使用旧库的程序已自动修复。

TT/S 最小修复位于 `if (initField.value()==1)` 内，内部赋值完成后、写出前：

```cpp
TT.correctBoundaryConditions();
S.correctBoundaryConditions();
```

原先 TT/S 内部场和物理边界相同，但 processor 缓存最大差分别为 2.37867584 和 0.32004831。实际 `patchNeighbourField()` 不自行交换，插值直接读缓存；`updateParameters` 又先用 TT/S 的完整字段计算 Cp、kk、Ds、rhok，所以该差异不能视为无害。r2 全场已确认这两组缓存恢复一致。

证据：[缓存消费链](C:/Users/15225/Documents/Codex/2026-08-31/n/work/amr-scalar-cache-aligned-20261024-r2/CACHE_USE_EVIDENCE.json)、[实际修复源码](C:/Users/15225/Documents/Codex/2026-08-31/n/work/amr-scalar-cache-aligned-20261024-r2/src/createFields.H)、[最终修复代码门禁](C:/Users/15225/Documents/Codex/2026-08-31/n/work/amr-scalar-cache-aligned-20261024-r2/recovery-01/build/FINAL_SQR_CODE_VERIFIED.json)。

## 📊 全 960 分区的覆盖与结果

诊断计算为作业 8743419，只有 `Time=0.001` 一个科学时间步。其全部 960 捕获完成，数据总计 934428804448 字节。独立只读分析作业 8743829 在 lidelei 使用 60 MPI、10 节点，调度 DONE，60 个分析 worker 实测退出码均为 0；960 个分区在配对比较、两臂覆盖审核、两臂独立残差及 CSR 系数检查六个类别中全部齐全。分析耗时约 2453 秒，不是求解器耗时。

分析同时检查目标机大端数据、CSR 缺少矩阵 flags 的兼容路径和重复列处理，再处理真实数据；本地联合测试还覆盖两边同时漏空矩阵边界、NaN、错误 patch 身份和错误量纲等拒绝路径。共 13351 组公共数组，全部有限；13 类已知表示/控制字典差异之外，没有意外 schema 差异。

| 检查 | 覆盖 | 结果 |
|---|---:|---|
| 初始/AMR 后五标量 | 960 分区，内部和全部 patch | 逐值相同 |
| 几何、拓扑、AMR 映射 | 960 分区全部选定记录 | 逐值相同 |
| 首个启动/加密后 pcorr | 公共 raw/effective A、b、初值、边界 | 逐值相同 |
| CSR 表示 | 160368330 行、942975414 个非对角项 | 全部严格相同 |
| 终场 | U、p_rgh、alpha.water、TT、S | 全有限，误差见下表 |

CSR 检查不是三个向量抽样：471487707 个 LDU 内部面展开成全部 942975414 个 `(row,col,value)` 条目，再与 CSR 排序逐项比较。不等组、重复列、非有限值、越界地址和 CSR 对角条目均为 0。范围是本次 `main_pressure.1.effective`，不外推到没有捕获的其他矩阵。

最终内部场误差如下，REL_L2 以原版完整场为分母，最大绝对差使用原字段单位。范数为不做体积加权的离散分量 L2，没有压力均值平移、事后缩放或剔除异常点。

| 场 | REL_L2 | 最大绝对差 |
|---|---:|---:|
| U | 2.646552e-6 | 0.003087161 |
| p_rgh | 9.527332e-5 | 5851.144923 |
| alpha.water | 2.098183e-6 | 0.004009005 |
| TT | 3.541723e-11 | 3.737166e-6 |
| S | 0 | 0 |

物理边界误差单列；processor 耦合边界另有完整结果，不能重复当作独立物理单元。

| 场 | 物理边界 REL_L2 | 最大绝对差 |
|---|---:|---:|
| U | 4.190162e-3 | 9.992664416 |
| p_rgh | 6.066252e-5 | 9416.437099 |
| alpha.water | 1.523391e-7 | 3.134424e-5 |
| TT | 4.545555e-11 | 1.159183e-6 |
| S | 0 | 0 |

温度内部 REL_L2 从前一修复版约 1.70674e-7 降到 3.54172e-11；这支持新增初始化同步的实际效果，不等于所有其余差异也由这两行代码造成。

证据：[终场完整结果](C:/Users/15225/Documents/Codex/2026-08-31/n/work/amr-firstdiv-20261023/io/r2-unified-20261024-01/final-results/summary.end.json)、[初场及组装输入](C:/Users/15225/Documents/Codex/2026-08-31/n/work/amr-firstdiv-20261023/io/r2-unified-20261024-01/final-results/summary.supplement.json)、[覆盖审核](C:/Users/15225/Documents/Codex/2026-08-31/n/work/amr-firstdiv-20261023/io/r2-unified-20261024-01/final-results/coverage.audit.json)、[矩阵和历史检查点边界审核](C:/Users/15225/Documents/Codex/2026-08-31/n/work/amr-firstdiv-20261023/io/r2-unified-20261024-01/final-results/boundary-matrix.audit.json)。

## 🔍 真实残差和迭代轨迹

使用独立标量 LDU 累加及 processor 邻接值重放实际算子，不复用 CSR 乘法代码。两臂各三套矩阵的 Ax0/Ax1/Ax2 全场最大差和相对范数均为 0；Ax1/Ax2 均有全局非零输入覆盖，未把零向量测试当成证明。

返回解不缩放、不改规范，直接计算 `b-Ax`。全 rank 的绝对值和只除以一份已全局规约的 `normFactor`，不把 960 份全局因子再相加。

| 受检首解 | 原版真实残差 | r2 真实残差 | 实际停止要求 |
|---|---:|---:|---|
| 启动 pcorr | 9.702176e-6 | 3.557672e-6 | 绝对 1e-5，relTol=0 |
| AMR 后 pcorr | 9.931900e-6 | 6.914520e-6 | 绝对 1e-5，relTol=0 |
| 首个主压力 | 0.02062788524 | 0.006213356964 | 1e-7 或初残差×0.05 |

六项都满足各自实际停止要求，与日志最终残差的最大绝对差约 2.14e-14。首个主压力初始残差约 0.422776，允许的相对停止门槛约 0.0211388；两边满足该相对门槛，**并没有达到绝对 1e-7**。不能说“日志写收敛就证明绝对精度相同”，也没有在本次诊断中放宽任何容差。

```mermaid
flowchart LR
    accTitle: Approximate Solve Trajectory Differences
    accDescr: 修复后首个pcorr的方程相同，但DIC和GAMG在允许残差处返回不同近似解，后续通量和主压力组装因此仍可略有不同。
    same_pcorr["相同首个 pcorr 方程"] --> different_preconditioner["DIC 与 GAMG 求解"]
    different_preconditioner --> allowed_solutions["各自满足停止门槛"]
    allowed_solutions --> changed_flux["校正通量略有不同"]
    changed_flux --> downstream_equations["后续主方程略有不同"]
```

首个主压力的 x0 仍逐值相同；effective 对角、非对角和 b 的 REL_L2 分别约 5.724e-8、5.555e-8、2.938e-7。该阶段两个求解器已经沿不同允许近似解推进，不能要求后续组装也逐值相同。与此同时，每个 CSR 条目均已与其自己的 LDU 原矩阵闭合，不能把此处两臂输入差异归咎于存储转换。

证据：[独立算子及残差](C:/Users/15225/Documents/Codex/2026-08-31/n/work/amr-firstdiv-20261023/io/r2-unified-20261024-01/final-results/true-residuals.json)、[残差与实际日志逐组核对](C:/Users/15225/Documents/Codex/2026-08-31/n/work/amr-firstdiv-20261023/io/r2-unified-20261024-01/final-results/true-residuals-vs-logs.json)、[全 CSR 条目核验](C:/Users/15225/Documents/Codex/2026-08-31/n/work/amr-firstdiv-20261023/io/r2-unified-20261024-01/final-results/csr-coefficients.json)。独立残差审核覆盖两臂各三个受检首解，不冒称每次后续压力调用均保存并独立核验过矩阵。

## ⚠️ 剩余最大差及解释限制

顶部速度最大差位于 rank841、patch3 面3184、相邻单元109518。两臂捕获几何给出同一法向 `(0,1,0)` 和面积 265.0238948。原版最终 phi=+0.01879959866，采用出流外推，边界 Ux=9.992664416；r2 phi=-0.005570768822，采用入流法向投影，边界 Ux=0。

该单元两臂内部 Ux 只差约 5.58e-9。分别把各自内部速度和通量符号代入正确边界分支，三个分量的预测误差都为 0。因此这个最大边界差不再是错误的 yz 混合，而是实际分支切换。phi/面积约为 +7.09e-5 和 -2.10e-5，不能称为机器零，也不能称这项边界差已消失。这里只逐分量闭合了全局最坏面，不假称逐面重算所有顶部公式。

内部压力最大差处，原版约 -7019236.95，r2 约 -7013385.81；局部差约为原值绝对值的 0.0834%。alpha 最大差处则为 0.731957105 与 0.735966110，差约 0.004009。这些原始点值保留，不能只用较小的整体范数替代局部判断。

另在 rank369/cell949，原版/r2 首个主压力 raw b 分别为 19.2189204355、19.2189089079；差异几乎完全由实际 phiHbyA 的有向面通量积分变化解释，隐含非正交/参考源差仅约 1.7e-10。rank426 的指定面，两边 `coefficient/rAUf` 同为约 1.644666666666707。上述局部闭合支持组装遵循各自当前输入，并不是“每个后续误差均已唯一归因于预条件器”的形式证明。

证据：[最终 r2 最坏点、边界公式及组装分项重放](C:/Users/15225/Documents/Codex/2026-08-31/n/work/amr-firstdiv-20261023/R2_REMAINING_HOTSPOTS-01.jsonl)。未开展多步稳定性、网格收敛或物理误差容许度标定。

## 💾 可恢复版本与复查入口

最终程序所在服务器目录：

```text
/home/export/online1/mdt00/shisuan/swpku_of/lnj/build/amr-scalar-cache-aligned-20261024-r2
```

程序是 `bin/boussinesqFoam-capture-wave-sqr-scalar-aligned`，SHA256 为 `3c522ea5cfdb54d47b6bce2fa315fac38974e005a74839bf419314a335b1dd95`。新入口源和对象、编译命令、链接 map、强符号修复对象、波浪库来源与全部实际链接依赖均可追溯；88 项实际依赖/对象保存了路径、真实路径、尺寸及 SHA256。原版、旧 CSR、wave-only、r1 及所有历史捕获保留，未覆盖参考树。

在服务器只读核验现有版本，不会触发新计算：

```bash
cd /home/export/online1/mdt00/shisuan/swpku_of/lnj/build/amr-scalar-cache-aligned-20261024-r2
sha256sum -c OUTPUTS.sha256
sha256sum -c REUSED_INPUTS.sha256
```

完整科学捕获位于 `lnj/tests/amr-firstdiv-20261023-r1/csr-wave-sqr-scalar-aligned/capture`，原版捕获在同根 `native/capture`。联合分析结果在 `lnj/analysis/amr-r2-unified-20261025-01/results`。诊断捕获模式在写普通时间目录之前结束，故没有常规 `0.001` 输出目录并非数据丢失；没有打开捕获环境时走普通入口，但本次不把未验证的多步运行称为已通过。

本地恢复摘要有 23 个小文件，根已独立逐文件复核下载清单的尺寸和 SHA256。证据：[恢复清单](C:/Users/15225/Documents/Codex/2026-08-31/n/work/amr-scalar-cache-aligned-20261024-r2/recovery-01/FETCH_MANIFEST.json)、[实际链接依赖记录](C:/Users/15225/Documents/Codex/2026-08-31/n/work/amr-scalar-cache-aligned-20261024-r2/recovery-01/build/RECOVERY_FILE_MANIFEST.json)、[r2 构建与机器码审核](C:/Users/15225/Documents/Codex/2026-08-31/n/work/amr-scalar-cache-aligned-20261024-r2/ROOT_STATUS-05.log)。

[原样保存的构建脚本](C:/Users/15225/Documents/Codex/2026-08-31/n/work/amr-scalar-cache-aligned-20261024-r2/build.sh)和[入口来源清单](C:/Users/15225/Documents/Codex/2026-08-31/n/work/amr-scalar-cache-aligned-20261024-r2/SOURCE_PROVENANCE.json)用于追溯重建；脚本有一次性 guard，不应原地重复执行。恢复仍依赖服务器已有神威环境和清单中保留的路径，不是脱离这些依赖即可独立运行的便携包。

## 📌 运行记录限制与验收边界

流体作业 8743419 的调度记录已过期，不能恢复独立的调度退出码。预设的 `exit-rank` 文件也没有生成：实际 `ParRunControl` 析构先打印 `Finalising parallel run`，随后调用 `Pstream::exit(0)`，使包装器无法等到 `__real_main` 返回。已确认的事实是单步完成、960 捕获完整、正常并行结束日志和无未处理 fatal；不能把这些冒称为“960 个进程实测退出码均为 0”。只读分析作业 8743829 则有已保存的 DONE 和 60 份实测 worker 退出 0，两者严格区分。

三臂均出现相同的 `libutilityFunctionObjects.so` 加载警告。r2 实际链接包含静态 `libutilityFunctionObjects.a(writeObjects.o)`，没有未知 functionObject 类型 fatal。独立 capture 不依赖该 functionObject 写出，因此没有删除警告或用它解释数值差异。证据：[退出路径](C:/Users/15225/Documents/Codex/2026-08-31/n/work/amr-scalar-cache-aligned-20261024-r2/EXIT_PATH-05.log)、[终态事实审核](C:/Users/15225/Documents/Codex/2026-08-31/n/work/amr-scalar-cache-aligned-20261024-r2/recovery-01/case/TERMINAL_RECOVERY_AUDIT.json)、[警告对照](C:/Users/15225/Documents/Codex/2026-08-31/n/work/amr-scalar-cache-aligned-20261024-r2/WARNING_COMPARISON-01.log)。

本次证明的是首因修复、已捕获方程/算子一致性和完整首步的定量差异；没有证明 DIC 与 GAMG 返回同一近似解、全部边界逐点一致、绝对压力残差达到 1e-7、所有后续矩阵均无误、多步结果可互换，或任何加速指标已经达成。新增物性更新前后五个捕获字段只有 candidate，已独立做量纲/有限值/边界覆盖审查，未捏造不存在的 native 同阶段对照。

报告按文档技能的 issue/修复审核结构组织，所有结论只引用本项目实测证据；图示保持文本源，未声称已完成外部渲染或 GitHub 提交。

2026-10-25，用户在获知数值审核结果和独立退出码记录缺口后，明确选择“保留限制，结束诊断”。因此本次数值诊断结案，不为补记录追加计算；上述运行记录限制和科学结论边界继续有效，不改写为不存在的退出码证明。

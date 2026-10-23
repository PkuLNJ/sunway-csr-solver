# CSR发展算例：新吞吐研究线，2026-10-23

范围：只读已有本地源码和完整日志，新增本地私有实现/测试；没有SSH、远端编译、作业提交、生产/参考树修改或算例复制。祖先及work/outputs内未找到适用AGENTS.md。与首次细化后pcorr分叉诊断独立。

结论：先研究当前**native U的三分量吞吐**，而非再写一遍压力GAMG/SGS缓存。alpha确有每步9秒的大块，发现一个可直接删的无用快照，但现有计时不足以声称它很贵。三个候选可以组合，尚无神威收益实测；没有证据支持本轮已达到20%。

## 证据及口径

唯一性能基线仍为n80 Job8677513，完整50步Loop2379.457611、native Clock2541，binary SHA `be97dddf6c282ee839ed2dfa5948c7acc59c39d075de799c6dd2ef4df6aa4c82`。固定用户2200秒基准对应目标1833.333秒，当前Clock差707.667秒。不能用Loop直接抵扣Clock。

`analyze.py`新提取同一份 `work/opt960-mainline50-20261020-01/followup-01/evidence-01/raw.log`，`TIMINGS.json`保存日志SHA及逐步值；2–49的Loop总数/压力轮数与已有COUNTS_TIMERS_V2逐项断言一致。以下都是这个固定窗口，不以早期窗口替换验收：

本文提到2–49约11.73秒/步差距时，只用既有x86同窗Clock2041/48=42.520833、其1.2倍目标35.434028，与CSR Clock2264/48=47.166667比较；这是分项研究的同窗参考，最终验收仍固定2200/1.2的完整50步目标。

| 区间内项目 | 秒/步 | 计数/事实 |
|---|---:|---|
| Loop | 47.150800 | 48步2263.238393秒 |
| 压力 | 26.669336 | 864系统、3015轮 |
| alpha | 9.018968 | 144次smoothSolver均1轮；432次MULES；288次相分数统计 |
| U总计 | 6.357477 | Ux/Uy/Uz各144次，全部1轮 |
| U build | 2.776018 | MRF仅0.000008秒/步；主要是方程组装 |
| U solve段 | 3.581395 | 包含reconstruct RHS、solve及fvOptions.correct，不是纯线性迭代 |
| T / S | 1.461484 / 1.574123 | S有28次0轮；不应与U跨方程批处理 |

完整50步alpha447.498241秒、U solve段178.571752秒。alpha2–10均9.021308，41–49均9.006263；这里没有后期自动消失的alpha成本。U solve段从3.451827到3.612318，只能描述走势，不能以此归因MPI。

重要源码纠正：当前n80不是“所有方程CSR”。`outputs/sunway-fixed960-source-20261016/cpe/source/solver/UEqn.H:57`仍native `solve(UEqn == reconstruct(...))`，完整log的U是 `DILUPBiCGStab`。`work/manual-evidence/bai/.../solver-csr/UEqn.H`是另一份旧源码，不可当当前vector solve入口。n80 build.sh:17/25钉住复用entry对象SHA `e5edd01a92a82710d82322693faf2d102311b8267b77d567c3470699807abcab`；本轮没有重新重建该对象的完整头文件闭包。私有补丁针对可得frozen header，有SOURCE_PINS，不能声称已集成到n80 ELF。

## 三个候选

### 1. Native U三分量锁步PBiCGStab + 三路DILU/Amul数据复用（优先）

实际入口为上述UEqn.H；可审查的native `fvMatrixSolve.C::solveSegregated` 位于 `work/pressure-trend-audit-20261020/source-01/x86-snapshot/src/finiteVolume/fvMatrices/fvMatrix/fvMatrixSolve.C:141–208`。它逐分量复制psi/source/边界系数，调整diag，构造solver，求解，写回该分量，再恢复diag。该文件是x86源码快照，不谎称其历史Sunway库对象已逐字验证。

实际Sunway移植实现可读于 `work/manual-evidence/bai/src/OpenFOAM/matrices/lduMatrix/solvers/PBiCGStab/PBiCGStab.C::solve`：每分量初始Amul(:101)、normFactor(:108)、初始norm(:117)，rho(:162)、DILU(:217)、Amul(:220)、r0Ay(:222)、s范数(:254)，必要时第二DILU/Amul和omega/最终norm(:288–332)。同目录DILU `calcReciprocalD` 的面依赖扫描(:82)、`precondition`的losort前扫(:150–154)、原面序逆扫(:156–159)仍主核；从核仅覆盖rD*r(:122–144)。**不能因已有CPE说整个DILU已加速。**

改法：在private native U专用vector路径把三个独立标量递推锁步执行。每分量保持自己的diag（含其BC）、rD、初始psi、normFactor、initialResidual/relTol参考、alpha/omega、singularity、计数及停止；不把向量方程改成block Krylov，不令某分量超额迭代。共享owner/neighbour/losort/lower/upper读取，把同阶段的三个标量dot/norm放入一次向量规约，Amul打包三分量halo并在一个面扫描中计算三个输出。分量diag即使不同也必须正确，不能无比较复用同一rD。

具体实施设计见 `U_BATCH_DESIGN.md`。已实现最敏感的DILU依赖核心 `DiluBatch3.H`：每个分量保留原face/losort/reverse次序；面为外层、分量为内层，降低重复索引/系数流量，支持active mask。与独立scalar参考共2,106,240次位级比较PASS（120图、8种mask、空图、独立分量diag、非对称lower/upper，g++ -O3 -ffp-contract=off）。这是可运行局部实现，不是Foam/MPI/CPE正确性证明。尚未实现完整锁步solver/halo，不把DILU单项测试扩大为整求解器等价。

收益证据边界：现用U全是1轮，不能靠“再降几轮”获益。若完整U solve段理想加速2倍最多省1.790698秒/步；即便该段完全归零，最多省3.581395秒/步（完整50步178.572秒）。这些是父计时包络，**不是实现会覆盖全部solve段或可达到的上限预测**。DILU/Amul/规约独立占比未知。三个分量同一阶段规约3→1是调用数条件，不等于延迟或总时间减为1/3。仅做DILU3核心但保留串行scalar外层不能获得批处理收益。

风险/验证：真实U边界可能有分量耦合/旋转变换/回调；按现用patch类型白名单和接口行为审查后才启用，不支持走原路径。新的规约packing会改变MPI树和舍入，需逐分量原normFactor/残差/停止、真实b−Ax、U及后续phi/压力质量审查。不得因为本次450次U调用均1轮硬编码“只做一轮”，也不得推断这些都走s提前返回分支。

### 2. Alpha只在aCorr>0时构造alpha10快照（低风险局部补丁）

`outputs/.../cpe/source/VoF/alphaEqn.H:183`每轮创建完整 `volScalarField alpha10("alpha10", alpha1)`，唯一读在:204的 `aCorr!=0`分支。现nAlphaCorr2、nOuterCorrectors3、nAlphaSubCycles1，因此每步6份快照中3份没有值用途。`MULES::correct`仍原次数和原参数；后续混合与rhoPhi也保留。不是n54/n80压力wA/psi保留优化。

已生成 `alphaEqn.lazy-snapshot.H/.patch`：`tmp<volScalarField>`只在aCorr!=0构造，else读取该快照；没有跨alpha调用缓存。每步快照6→3，至少少三次本地internal数组复制；n80 rank0 rows166632时，单份scalar internal约1.27MiB，但不能拿该rank大小乘960宣称准确总流量，实际边界/字段对象开销也未知。

收益包络：快照是9.018968秒alpha父计时的一部分，没有独立耗时；可支持下界为0（可能极小），不能宣称alpha减半。即把alpha整体2倍也仅省4.509484秒/步，整项删除仅是9.018968秒/步的荒谬包络。不能把快照省一半推成alpha省一半。

验证：语义序列模型1/2/3/8个corrector结果逐步相同，每次alpha调用少1快照。真实Foam尚未编译；检查tmp指针赋值API、原快照IO/registry生命周期以及实际MULES是否有依赖该对象名的自定义回调。比较每次alpha/MULES后的完整场、alphaPhi10/rhoPhi、体积加权相体积、界面误差、越界体积及后续守恒。保持tol、nLimiterIter3、nAlphaCorr2及所有压缩设置。

### 3. 非正交循环只计算一次不变的div(phiHbyA)

`outputs/.../cpe/source/solver/pEqn.H:65`在每次非正交校正重新 `fvc::div(phiHbyA)`；phiHbyA在进循环前已确定，直到finalNonOrthogonalIter才更新输出phi，而非phiHbyA。现6次pEqn调用/步，每次3次非正交solve，因此该显式div可18→6次/步。每一轮仍重新执行laplacian（包括依赖当前p_rgh的非正交修正）、setReference、solve及最终post。

`pEqn.hoisted-div.H/.patch`已在循环前创建 `const tmp<volScalarField> tphiHbyADiv`，循环内只传const field引用，不把tmp转移/消费。作用域仅一回pEqn调用，无跨corrector或网格代际缓存。与既有exact operator bundle不同：后者仍每轮fresh assembly/比较，未删这项重复显式div；本候选也可与它组合，但旧bundle已测收益不能重复记入。

收益：计入已有p_rghEqn build3.810966秒/步，具体div份额未知。仅依据计时的安全粗包络小于等于该整段3.810966；若假设每次div等成本且其余项非负，18→6给出更乐观条件上限2.540644秒/步，但并非实测保证，真实一般更小。不能把3.81秒整段都称CSR转换。保留一个div field会延长其生命周期/增加峰值live内存，需测该额外驻留。

验证：先确认目标fvc::div纯读phiHbyA及固定mesh/离散配置，无自定义有状态scheme/回调；本地未获取其完整实际库实现，不能称已证全部等价。序列mock刻意让非正交修正和reference随每轮p变化，hoist只影响显式div，序列一致。目标需比较每次raw source、normFactor、残差/轮数及全场。**不合并两次p_rghEqn.flux()**：它们跨越p_rgh.relax，不能假定值相同；私有补丁保留两次。

## 组合与20%目标

三项分别覆盖U求解吞吐、alpha快照和pressure显式div，不重算同一父子计时。均可与n80既有pressure SGS/topology/numeric-pack/RHS修复组合；未实现/未测的“组合”不是已交付binary。

即假设U solve段2倍、整个alpha2倍，再将整个p_rgh build按理想2/3删去（实际不成立），也只对应约8.84秒/步条件收益，仍不足现同窗约11.73秒缺口。更不能据此把完整50Clock从2541保证降至1833.333。现有证据明确不支持把任一小改装成20%方案；应先将U细分计时/源码集成做实，再决定是否投入完整锁步重构。Alpha dead-copy、div hoist适合作为成本很低的组合项。

旧方案排重：已读n26/n29 exact pressure bundle、n7 workspace、n38/39 zero-halo/discarded coarse、n43 exact throughput composition、n37 first-PCG product batching及n78/n80 runtime组成；这里没有再次提出它们或将其收益加入。未重复SGS重排/FP32/改变scale/skipcoarse/放松tol。native DIC首次分叉留给独立诊断线。

## 本地验证命令及结果

```
python work/csr-next-acceleration-20261023/analyze.py
python work/csr-next-acceleration-20261023/derive_private.py
python work/csr-next-acceleration-20261023/test_liveness.py
C:/msys64/ucrt64/bin/g++.exe -std=c++17 -O3 -ffp-contract=off -Wall -Wextra -Werror work/csr-next-acceleration-20261023/test_dilu.cpp -o work/csr-next-acceleration-20261023/test_dilu.exe
work/csr-next-acceleration-20261023/test_dilu.exe
```

已实际运行：统计交叉断言PASS；三个liveness测试PASS；DILU位比较PASS。没有本机性能数字或神威收益数；没有修改生产文件。

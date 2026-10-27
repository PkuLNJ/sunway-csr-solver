# 新曙光 FP64INT64 基准与神威优化版对齐

2026-10-27。用户要求新曙光版本跑50步保存终场；随后明确取消新基准与未对齐旧神威版的场比较，先对齐最新神威版。

## 曙光参考任务

- 作业125838341，kshcnormal，30节点/960 MPI，2小时上限。
- 独立目录：/public/home/chenzhi/lnj/qt-int64-960-50-20261027-01/case。
- 可执行文件及libseaWave.so已复制到父目录bin/lib，哈希BINARY.sha256；源码快照source。
- 来源：QT_OpenFOAM/FP64INT64/OpenFOAM-7；未重编曙光程序。
- 从0开始，dt=0.001，endTime=0.05，50步；writeControl=timeStep，writeInterval=50，writePrecision=17。
- writeObjects强制保存U、p_rgh、TT、S、alpha.water、p、rhok；结束后verify.py检查50步、End、致命错误和960×7个场文件，并写FINAL_CHECK.json。
- 不修改原作业125831723；只链接只读初始场/网格，TT和S采用私有文件。使用原环境、同一分区与进程布局；fvSolution/fvSchemes保留原文。
- 日志solver.125838341.log，返回码solver.125838341.rc。当前状态见STATUS04.log，提交时PENDING(Priority)。

## 已确认的行为差异

1. 新createFields.H启用U_z += aU*z+bU和U_x += aV*z+bV；当前最快神威版这两行被注释。initField=1，系数非零，属于实际初值变化。
2. 新seaWave的速度和波高相位不再使用波流耦合频率修正k(UMean_x*cos(theta)+UMean_z*sin(theta))，而使用2*pi/T。
3. 平均流由setVelocity最后加UMean，变为Uf内部先令u=UMean_x再参与fraction*tCoeff；界面部分湿润面和ramp阶段并不等价。
4. UMean成员由派生seaWave移至基类waveModel。必须按新头文件重编整个波浪库，不混合新旧对象。
5. 新createFields移除了TT/S初始化后的correctBoundaryConditions和初始write；为了匹配新参考，私有对齐版采用新createFields完整原文。此处不代表已验证新旧终场一致或认定新的参考行为无问题。
6. TEqn.H、SEqn.H、updateParameters.H、correctPhi.H、initCorrectPhi.H，与当前神威活动包含目录逐文件SHA256一致。

## 神威对齐版

- 私有构建目录：lnj/build/csr-qt-int64-aligned-20261027-01。
- 父版本alpha-mules-cpe-20261027-01，三次窗口1674/1703/1669秒是旧物理定义下的结果。
- 覆盖createFields.H为新参考；完整重编9个seaWave编译单元；重新编译求解器入口并链接。
- 复用原alpha CPE对象、计时对象、优化dfMatrices；保留minIter0保护、SGS、GAMG延迟构造、U/P几何复用、sqr正确性修复。
- 尚未提交对齐版流体作业，也没有声称对齐版数值验证通过；现有最快版不覆盖。
- 新旧源码差异意味着不能把新曙光2569秒与旧神威1682秒直接作为同题加速验收；也不能把时间增长全部解释为INT64。

证据位于work/qt-int64-50-20261027-01：DIFF.log、SW_EXACT.log、SW_WAVE_PROVENANCE.log、SW_BUILD01.log、new-source/FULL_MANIFEST.json及各状态文件。

## 编译完成核验（17:24后）

BUILD_EXIT_CODE=0，CONSTRUCTION_PROFILE_BUILD_COMPLETE存在。entry/wave/link日志未发现error/undefined reference/multiple definition。链接map确认9个波浪对象全部来自新私有libseaWave.a。新可执行6ed29e3f03a49e612d762cb3683a6ce4c160ab7ee4484f5f311dc0e3aea186b1。
新seaWave e12af506217112f9f114774402a5ec37e739fa2266bf72c6bde43d4f99d5f363；优化dfMatrices与父版哈希均为4c03ebfc31a0650a8d4ec5187c8d07095d89ba6817935dc4748db89544886164。
这里只确认编译/链接完成，尚无该新程序的计算节点启动、流体运行或新数值一致性证据。未提交新旧未对齐终场比较。
源码首次保全Git ca66033，已推送私有仓库；编译收据另附SW_BUILD_VERIFIED.log。

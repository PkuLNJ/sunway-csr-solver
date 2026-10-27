# QT对齐版CSR+GAMG，960分区50步
使用csr-qt-int64-aligned-20261027-01编译产物；保留CSR+GAMG和全部最新加速开关，非曙光DIC配置。
保留原神威发展算例输入，waveProperties/boussinesqProperties与新曙光字典去注释空白后相同。
动态网格参数一致；神威correctFluxes多一项(alphaPhi1Corr0 none)，是既有缓存通量登记，不改变加密阈值或规模。未宣称所有初场逐字节相同。
50步至0.05，第50步保存七场，writePrecision17；lidelei，960MPI/160节点，2小时运行上限；不比较未对齐的旧终场。

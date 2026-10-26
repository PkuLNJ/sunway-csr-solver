# 构造细计时实测：8753574

2026-10-26，lidelei，960 MPI，发展算例一步至0.001；minIter1、nFinestSweeps2。调度DONE、STOP_REQUESTED0，73条求解记录与既有参考逐条一致。全场比较8753635独立执行，未完成前不宣称全场数值通过。

| 范围 | 每进程平均调用次数 | 平均累计秒 | 最大累计秒 |
|---|---:|---:|---:|
|G_hierarchy_New|18|0.737467|0.752168|
|G_constructor_body|18|1.963300|2.184096|
|G_agglomerate_total|216|1.960343|2.181100|
|G_diag_restrict|216|0.051672|0.060870|
|G_interfaces_total|216|0.031457|0.049384|
|G_offdiag_aggregate|216|0.328747|0.377315|
|G_coarse_CSR|216|1.546535|1.747509|
|G_processor_aggregate|0|0.000000|0.000000|
|U_ddt|3|0.048082|0.056816|
|U_div|3|0.890721|0.918834|
|U_MRF|3|0.014419|0.015355|
|U_viscous|3|1.733285|1.788619|
|U_options|3|0.287459|0.371730|
|U_rhs_reconstruct|3|1.670655|1.702349|
|P_fine_CSR|18|1.179757|1.383612|
|G_interface_coeffs|216|0.029899|0.047640|

## 优化优先级

1. 统一优化csrMatrix::csrMatrix(const lduMatrix&)的结构生成。细网格18次1.179757秒，粗层216次1.546535秒，两者不重叠合计2.726291秒。由一个构造入口覆盖两个位置，比只改细网格收益空间更大。先分离拓扑映射/数值填充并保留精确顺序；不是直接缓存完整求解器。2.726秒是该范围总开销，不是可兑现节省。

2. U粘性应力项1.733285秒。运行模型确认laminar/Stokes，接口仍名turbulence。linearViscousStress::divDevRhoReff(rho,U)包含-fvc::div((alpha*rho*nuEff)*dev2(T(fvc::grad(U))))和-fvm::laplacian(alpha*rho*nuEff,U)。应检查重复系数字段生成，以及grad(U)/张量变换/div的数组遍历；不能因laminar而删去粘性项或将变系数当常数。

3. U显式右端重构1.670655秒（属U solve外围，不属U矩阵组装）：surfaceTensionForce、snGrad(rhok)、snGrad(p_rgh)、面乘法和reconstruct。应进一步辨别面量重复算、临时场与reconstruct本体；压力梯度随压力变化，不能跨修正轮直接缓存。

4. U对流项0.890721秒。优先检查插值/边界更新和矩阵填充；不建议先动0.048秒的时间项和0.014秒的MRF。

## GAMG构造解释

层级New0.737467秒+构造body1.963300秒，两段不重叠，合计2.700767秒。body几乎全在agglomerateMatrix：粗CSR1.546535、非对角聚合0.328747、对角聚合0.051672、界面0.031457，其余约0.002。粗CSR约占body79%。

processor aggregation本次调用为0：不能把当前耗时归咎于跨处理器粗网格聚合，也不能因为插件链接了就认为该路线在运行。

CSRGAMGAgglomeration::New源码有对象注册表缓存；0.737秒可能含首步新建或网格变化后的重建，未拆hit/miss，不能称18次全部重建、更不能外推每个后期步骤都花0.737秒。

## 解释限制

- 单步用于定位调用热点，不证明50步平均收益。
- G_agglomerate_total包含diag/interfaces/offdiag/coarseCSR，不能重复相加；G_interface_coeffs又包含在interfaces_total中。
- 表中为960rank累计时间均值与最大值，不是整个作业总时长；不同项最大值可能来自不同rank。
- U各项表达式计时不包含所有矩阵相加/赋值/临时对象释放，不能直接等同完整U_buildEqn。
- 源码去除新增计时后入口、UEqn表达式树、pEqn均恢复原文；运行73条求解记录相同。全场仍由8753635检查。
- 没有基于这份计时改变任何容差、迭代策略或生产源码。
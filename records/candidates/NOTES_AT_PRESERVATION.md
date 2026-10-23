# 上机测试记录（持续更新）

用户经主agent明确授权：两个小改独立对照n80，先960单步严格质量，再固定10步同窗；有数值与收益证据才合并。生产/参考树不变。

私有build `/lnj/build/csr-next-small-20261023`：只编译两份entry，未复制/重建n80 dfMatrices、native OpenFOAM或CPE依赖。alpha/div编译、链接均RC0，.ldm=85544/.slave_tls=40。完整SHA和当前作业见ACTUAL_STATE.json。

唯一复用slot为旧n80 step1 candidate，**不是**诊断使用的n80-lidelei10 candidate。已取得本任务专属锁；旧8671829的0.001输出按目录rename移至runtime/old-output-8671829，能恢复。只在初次准备恢复会被createFields写入的TT/S，其他0和constant仍沿用旧已验证的只读链接契约。没有新增全量case。

真实阻碍与修正：

- 首次提交base1从SSH默认base文件系统cwd被bsub拒绝，无jobID/RC255；保留回执，随后cd到实际case。
- base1b job8723338(q_share)实际运行但在OpenFOAM banner之前出现CPE exception type6/8，节点六MPE exit_code1，后进入EXITING。它不是数值或计时基准。本次提交遗漏历史n80的-m1及-K；已核实bsub -h中-m传入swrun，-K意为kill-on-bad-exit，尚不宣称缺项是唯一已证根因。核实精确自有job后清理该已失败RUN，确认EXIT；本地live-evidence/base1b保留原stdout/terminal/inputs/receipt。
- 恢复原-K -m1契约后，一次重提base1c job8723386至优先q_swlidelei。沿用未进入main的准备输入，未重置/重编。当前PEND；不会因计算walllimit把排队作业取消。

两个补丁仍需实际CFD质量证据，未合并。输出字段保留原n80 ASCII writePrecision8；1e-12比较门针对已有写出字段及打印诊断，不冒充所有内部double逐位证明。

计时审查：div hoist移到while外，原p_rghEqn build计时因此改变子段覆盖范围。效益只能看完整p Equations、Loop和同窗Clock，包括hoisted div本身；不能把子段表面下降全部记为删除的计算。所有臂同一运行窗和同输出设置；share单次波动不作为收益证据。

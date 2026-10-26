# Lifecycle experiments (2026-10-26)
Both private builds passed compile/link. Runtime numeric and timing validation pending.
Correction to earlier plan: workspace reuse ALREADY EXISTS in r2. Actual preconditioner archive member hash115da75922ad7e1ecd07c865550852f9c387fdfefccda899d223b015325e4005 equals fixed960 ancestor; binary has invalidateWorkspace symbol.
A: LNJ_LAZY_GAMG=0|1; only defer GAMG construction until PCG iteration needed; use minIter0.
B: LNJ_VCYCLE_RESET=full|minimal; reuse old audited prototype, remove three overwritten workspace zero traversals; use minIter1.
Both use fixed nFinestSweeps2, current r2 dependencies and zero-time print guard. Off/on tests are optimization-switch comparisons, NOT a host-versus-CPE speedup measurement. Separate host/CPE validation remains required before claiming that speedup.
Old reference source must not be used as proof of active implementation. No benefit yet measured; do not combine before numerical checks.

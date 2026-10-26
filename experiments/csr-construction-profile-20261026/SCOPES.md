# Construct profile scope map

Baseline r2 minIter1/nFinestSweeps2, only main p_rgh GAMG construction; pcorr excluded.
No algorithm settings changed. U subexpressions wrapped in forwarding timers without changing the operator tree; compiler expression evaluation can still change, so compare full solver trace/fields before calling timings representative.

Inclusive hierarchy:
- G_hierarchy_New: obtains/builds agglomeration, separate from constructor body.
- G_constructor_body includes controls/logging, all level builds, optional coarse direct preparation.
- G_agglomerate_total includes G_diag_restrict + G_interfaces_total + G_offdiag_aggregate + local G_coarse_CSR.
- G_interfaces_total includes allocations plus G_interface_coeffs.
- G_processor_aggregate includes processor gathering and its G_coarse_CSR.
- G_coarse_CSR collects conversions in both paths, do not add it again to parent totals.
- U_ddt/div/MRF/viscous/options: RHS expression evaluation only; additions and final assignment remain outside these subscopes. U_buildEqn prior timer still measures full expression.
- U_rhs_reconstruct: explicit RHS reconstruction, not the native linear solve.
- P_fine_CSR: fine pressure conversion only.

Rank-local totals; exactly three reductions at program end, no per-level barriers. Mean is across all960 ranks including inactive/zero-work; max is per-scope max and not necessarily same rank across scopes. Calls mean identifies coarse-rank participation. Do not add maxima into an invented critical path.
Source snapshot updated to exclude pcorr before relevant translation units compiled; initial SOURCES.sha256 predates this filter. Final source hash manifest must be saved separately after build.

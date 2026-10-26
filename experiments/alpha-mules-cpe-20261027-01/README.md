Base: B1 binary source plus runtime SGS CPE, job8757484 (1728s steps2-49). New feature LNJ_MULES_CPE=0/1.
New kernels cover cell limiter ratios and internal-face lambda updates only. Original scatter sums, boundaries and syncFaceList remain unchanged. Face coefficient cache is per dispatch; no persistent geometry cache.
Local portable kernel tests pass bitwise for 10 sizes and cache collisions. Target CPE compilation passes with const-qualification DMA warnings. Full solver build and target numerical gate pending; no performance claim.
Procedure: same new ELF OFF/ON one step each; strict seven fields gate; ON50 only if pass; compare final against8757484. No new OFF50.

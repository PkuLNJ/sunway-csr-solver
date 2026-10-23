#include <crts.h>
#include "lnjColoredSgsParam.h"

#define LNJ_COLORED_SGS_CPE_COUNT 64
#define LNJ_COLORED_SGS_TILE_ROWS 256

// A bounded 2 KiB output buffer, not one full partition per CPE. Each tile is
// sent by one contiguous DMA, avoiding per-row scattered DMA writes.
static __thread_local double lnj_colored_sgs_results[LNJ_COLORED_SGS_TILE_ROWS]
    __attribute__((aligned(64)));

void lnjColoredSgs_CPE(struct lnjColoredSgsParam* parameter)
{
    struct lnjColoredSgsParam p __attribute__((aligned(64)));
    crts_rply_t dma_reply = 0;
    unsigned int dma_count = 0;
    int64_t tid;
    int64_t count;
    int64_t quotient;
    int64_t remainder;
    int64_t start;
    int64_t stop;
    int64_t tile;

    CRTS_dma_iget(&p, parameter, sizeof(p), &dma_reply);
    ++dma_count;
    CRTS_dma_wait_value(&dma_reply, dma_count);

    tid = (int64_t)CRTS_tid;
    if (tid < 0 || tid >= LNJ_COLORED_SGS_CPE_COUNT
        || p.begin < 0 || p.end < p.begin)
        return;

    count = p.end - p.begin;
    quotient = count / LNJ_COLORED_SGS_CPE_COUNT;
    remainder = count % LNJ_COLORED_SGS_CPE_COUNT;
    // Remainder distributed one row per early CPE, including count<64.
    start = p.begin + quotient * tid + (tid < remainder ? tid : remainder);
    stop = start + quotient + (tid < remainder ? 1 : 0);

    for (tile = start; tile < stop; )
    {
        const int64_t remaining = stop - tile;
        const int64_t rows = remaining < LNJ_COLORED_SGS_TILE_ROWS
            ? remaining : LNJ_COLORED_SGS_TILE_ROWS;
        int64_t local;
        for (local = 0; local < rows; ++local)
        {
            const int64_t row = tile + local;
            int64_t entry;
            double sum = p.rhsPtr[row];
            for (entry = p.rowPtr[row]; entry < p.rowPtr[row + 1]; ++entry)
                sum -= p.offDiagPtr[entry] * p.psiPtr[p.colPtr[entry]];
            lnj_colored_sgs_results[local] = sum / p.diagPtr[row];
        }

        CRTS_dma_iput(p.psiPtr + tile, lnj_colored_sgs_results,
                      (unsigned long)(rows * (int64_t)sizeof(double)), &dma_reply);
        ++dma_count;
        CRTS_dma_wait_value(&dma_reply, dma_count);
        tile += rows;
    }
    // Host must join this spawn before launching another color. No row inside
    // this batch may depend on another row in [begin,end): helper validates it.
}

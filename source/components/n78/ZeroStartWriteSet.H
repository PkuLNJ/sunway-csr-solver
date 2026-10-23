#ifndef LNJ_N77_ZERO_START_WRITE_SET_H
#define LNJ_N77_ZERO_START_WRITE_SET_H
#include "BoundaryTilePlan.H"
namespace lnj {
struct ZeroStartWriteSet {BoundaryTilePlan plan;bool covered;};
// Constructor-only metadata. Actual tile starts already cover ALL MPI getter/
// updater cells; add original zeroStart meshIf writes, never replace actual set.
inline ZeroStartWriteSet zeroStartWriteSet(BoundaryTilePlan actual,
 const std::vector<std::int64_t>& zeroCells,bool listVerified){
 if(!listVerified)return {std::move(actual),false};
 auto unionCells=actual.starts;
 for(const auto c:zeroCells){if(c<0||c>=actual.rows)return {std::move(actual),false};unionCells.push_back(c);}
 return {BoundaryTilePlan(actual.rows,unionCells),true};
}
}
#endif

// Portable conservative admission policy. No Foam or MPI dependencies.
#ifndef lnjSmallCoarseEstimate_H
#define lnjSmallCoarseEstimate_H
#include <cmath>
#include <cstdint>
#include <limits>
namespace lnjSmallCoarse {
struct Counts {
    std::uint64_t rows=0, internalFaces=0, processorFaceSides=0;
    std::uint64_t restrictEntries=0, transformComponents=0, interfaces=0, ranks=0;
};
inline bool addTerm(std::uint64_t count,std::uint64_t bytes,std::uint64_t& total) {
    if(count>(std::numeric_limits<std::uint64_t>::max()-total)/bytes)return false;
    total+=count*bytes;return true;
}
inline bool estimate(const Counts& c,std::uint64_t& bytes) {
    // Conservative additional peak budget for retained originals, native proxies,
    // receive payloads, native combined mesh, CSR copy, maps, coarse vectors and
    // next-level storage. NOT total MPI-process RSS or a free-memory detector.
    bytes=1024*1024;
    return addTerm(c.rows,768,bytes)
        &&addTerm(c.internalFaces,1024,bytes)
        &&addTerm(c.processorFaceSides,1024,bytes)
        &&addTerm(c.restrictEntries,128,bytes)
        &&addTerm(c.transformComponents,128,bytes)
        &&addTerm(c.interfaces,4096,bytes)
        &&addTerm(c.ranks,32768,bytes);
}
inline bool admitted(const Counts& c,std::uint64_t cap,std::uint64_t& bytes) {
    return c.rows>0&&c.ranks>1&&estimate(c,bytes)&&bytes<=cap;
}
} // namespace
#endif

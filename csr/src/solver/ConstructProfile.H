#pragma once
#include <mpi.h>
namespace lnjConstruct {
void add(int,double);
struct Scope { int id; double start; explicit Scope(int i):id(i),start(MPI_Wtime()){} ~Scope(){add(id,MPI_Wtime()-start);} };
template<class F> auto timed(int id,F f)->decltype(f()){Scope t(id);return f();}
void report();
}

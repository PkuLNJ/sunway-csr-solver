#include "ConstructProfile.H"
#include <cstdio>
namespace lnjConstruct {
static double seconds[16]={};static double calls[16]={};
void add(int id,double t){if(id<0)return;seconds[id]+=t;calls[id]+=1;}
void report(){int rank,size;MPI_Comm_rank(MPI_COMM_WORLD,&rank);MPI_Comm_size(MPI_COMM_WORLD,&size);
double sums[16],maxs[16],counts[16];
MPI_Reduce(seconds,sums,16,MPI_DOUBLE,MPI_SUM,0,MPI_COMM_WORLD);
MPI_Reduce(seconds,maxs,16,MPI_DOUBLE,MPI_MAX,0,MPI_COMM_WORLD);
MPI_Reduce(calls,counts,16,MPI_DOUBLE,MPI_SUM,0,MPI_COMM_WORLD);
const char* names[]={"G_hierarchy_New","G_constructor_body","G_agglomerate_total","G_diag_restrict","G_interfaces_total","G_offdiag_aggregate","G_coarse_CSR","G_processor_aggregate","U_ddt","U_div","U_MRF","U_viscous","U_options","U_rhs_reconstruct","P_fine_CSR","G_interface_coeffs"};
if(rank==0)for(int i=0;i<16;i++)std::printf("LNJ_CONSTRUCT %s calls_mean=%.9g seconds_mean=%.17g seconds_max=%.17g\n",names[i],counts[i]/size,sums[i]/size,maxs[i]);
}
}

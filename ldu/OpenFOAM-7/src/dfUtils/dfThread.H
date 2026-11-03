#pragma once

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef __sw_64__
#include <crts.h>
#endif


inline int df_max_threads(){
#ifdef __sw_64__
    return CRTS_MAX_SPE_NUM;
#else

#ifdef _OPENMP
    return omp_get_max_threads();
#else
    return 1;
#endif

#endif
}

inline int df_thread_size(){
#ifdef _OPENMP
    return omp_get_num_threads();
#else
    return 1;
#endif
}

inline int df_thread_rank(){
#ifdef _OPENMP
    return omp_get_thread_num();
#else
    return 0;
#endif
}
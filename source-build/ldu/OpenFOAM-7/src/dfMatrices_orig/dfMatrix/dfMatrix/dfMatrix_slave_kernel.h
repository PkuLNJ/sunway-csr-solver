#pragma once

#include <crts.h>
#include "dfMatrix_slave_param.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void SLAVE_FUN(dfMatrix_Block_CSR_SpMV_CPE)(dfMatrix_Block_CSR_SpMV_param_t* para_p);

extern void SLAVE_FUN(dfMatrix_Block_CSR_GaussSeidel_CPE)(dfMatrix_Block_CSR_GaussSeidel_param_t* para_p);

// extern void SLAVE_FUN(dfMatrix_Block_CSR_sumA_naive)(dfMatrix_Block_CSR_sumA_param_t* para_p);

// extern void SLAVE_FUN(dfMatrix_Block_CSR_GuassSeidel_naive)(dfMatrix_Block_CSR_GuassSeidel_param_t* para_p);

// extern void SLAVE_FUN(dfMatrix_Block_CSR_value_copy_navie)(dfMatrix_Block_CSR_value_copy_param_t* para_p);

#ifdef __cplusplus
}
#endif
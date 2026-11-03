#pragma once

#include <crts.h>
#include "csrMatrix_slave_param.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void SLAVE_FUN(csrMatrix_SpMV_CPE)(csrMatrix_SpMV_param_t* para_p);

#ifdef __cplusplus
}
#endif
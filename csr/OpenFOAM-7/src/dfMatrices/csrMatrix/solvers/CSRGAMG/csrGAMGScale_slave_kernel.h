#pragma once

#include <crts.h>
#include "csrGAMGScale_slave_param.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void SLAVE_FUN(csrGAMGScale1_CPE)(csrGAMGScale1_param_t* para_p);

extern void SLAVE_FUN(csrGAMGScale2_CPE)(csrGAMGScale2_param_t* para_p);

#ifdef __cplusplus
}
#endif

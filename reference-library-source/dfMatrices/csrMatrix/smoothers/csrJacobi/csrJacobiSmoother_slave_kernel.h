#pragma once

#include <crts.h>
#include "csrJacobiSmoother_slave_param.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void SLAVE_FUN(csrJacobiSmoother_CPE)(csrJacobiSmoother_param_t* para_p);

#ifdef __cplusplus
}
#endif
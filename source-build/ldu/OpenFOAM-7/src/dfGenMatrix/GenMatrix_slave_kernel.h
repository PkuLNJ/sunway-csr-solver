#pragma once

#include <crts.h>
#include "GenMatrix_slave_param.h"

#ifdef __cplusplus
namespace Foam{
extern "C" {
#endif

extern void SLAVE_FUN(build_UEqn_intern_1_CPE)(GenMatrix_UEqn_param_1_t* para_p);
extern void SLAVE_FUN(build_UEqn_intern_2_CPE)(GenMatrix_UEqn_param_2_t* para_p);

#ifdef __cplusplus
}
}
#endif

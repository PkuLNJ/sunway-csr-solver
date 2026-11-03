#pragma once

#include <string>
#include <cmath>

#ifdef __sw_host__
typedef short float16;
#endif

// void check_nan(const double* array, int64_t len,const std::string& name);
// void check_nan(const float* array, int64_t len,const std::string& name);
// void check_nan(const float16* array, int64_t len,const std::string& name);

// void check_overflow(const double* array, int64_t len,const std::string name);

// void check_fp16_error(const double* array, int64_t len,const std::string& name);
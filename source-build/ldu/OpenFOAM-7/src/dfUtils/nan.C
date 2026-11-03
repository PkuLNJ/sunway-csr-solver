#include "nan.H"
#include <messageStream.H>
// #include <swfp16.h>
#include "memory.H"
#include <mpi.h>

#define RELATIVE_ERROR_TOLERANCE 1e-3
#define ABSULTE_ERROR_TOLERANCE 1e-3
#define ZERO_TOLERANCE 1e-3

// void check_nan(const double* array, int64_t len,const std::string& name){
//     Foam::Info << "In check_nan " << name << " : ---------------------------" << Foam::endl;
//     for(int64_t i = 0; i < len; ++i){
//         if(std::isnan(array[i])){
//             Foam::Info << name << " " << i << " : " << array[i] << Foam::endl;
//         }
//     }
//     Foam::Info << "--------------------------------------------------------------" << Foam::endl;
// }


// void check_nan(const float* array, int64_t len,const std::string& name){
//     Foam::Info << "In check_nan " << name << " : ---------------------------" << Foam::endl;
//     for(int64_t i = 0; i < len; ++i){
//         if(std::isnan(array[i])){
//             Foam::Info << name << " " << i << " : " << array[i] << Foam::endl;
//         }
//     }
//     Foam::Info << "--------------------------------------------------------------" << Foam::endl;
// }


// void check_nan(const float16* array, int64_t len,const std::string& name){
//     Foam::Info << "In check_nan " << name << " : ---------------------------" << Foam::endl;
//     double* tmp = (double*)df_alloc(len * sizeof(double));
//     fp16_array_to_double_array(tmp, array, len);
//     for(int64_t i = 0; i < len; ++i){
//         if(std::isnan(tmp[i])){
//             Foam::Info << name << " " << i << " : " << tmp[i] << Foam::endl;
//         }
//     }
//     df_free(tmp);
//     Foam::Info << "--------------------------------------------------------------" << Foam::endl;
// }

// void check_overflow(const double* array, int64_t len,const std::string name){
//     Foam::Info << "In check_overflow " << name << " : ---------------------------" << Foam::endl;
//     for(int64_t i = 0; i < len; ++i){
//         float abs = std::abs(array[i]);
//         if(abs != 0. && (abs < 1e-4 || abs > 1e4)){
//             Foam::Info << name << " " << i << " : " << array[i] << Foam::endl;
//         }
//     }
//     Foam::Info << "--------------------------------------------------------------" << Foam::endl;
// }


// void check_fp16_error(const double* answer, int64_t len,const std::string& name){
//     float16* fp16 = (float16*)df_alloc(len * sizeof(float16));
//     double* check = (double*)df_alloc(len * sizeof(double));

//     fp16_array_from_double_array(fp16, answer, len);
//     fp16_array_to_double_array(check, fp16, len);
    

//     bool check_faild = false;
//     double max_relative_error = 0.;
//     double max_absulte_error = 0.; 
//     for(int64_t i = 0; i < len; ++i){
//         double absulte_error = std::abs(answer[i] - check[i]);
//         double relative_error = std::abs(answer[i]) < ZERO_TOLERANCE ? absulte_error : (absulte_error / std::abs(answer[i]));
//         if(absulte_error > ABSULTE_ERROR_TOLERANCE && relative_error > RELATIVE_ERROR_TOLERANCE){
//             Foam::Info << name << " error : " << Foam::endl;
//             Foam::Info << "answer[i] : " << answer[i] << Foam::endl;
//             Foam::Info << "check[i] : " << check[i] << Foam::endl;
//             Foam::Info << "absulte_error : " << absulte_error << Foam::endl;
//             Foam::Info << "relative_error : " << relative_error << Foam::endl;
//             check_faild = true;
//         }
//         if(relative_error > max_relative_error){
//             max_absulte_error = absulte_error;
//             max_relative_error = relative_error;
//         }
//     }

//     if(check_faild){
//         MPI_Abort(MPI_COMM_WORLD, -1);
//     }else{
//         Foam::Info << "In fp16_error check pass : " << name << "-------------------------------------" << Foam::endl;
//         Foam::Info << "max_absulte_error : " << max_absulte_error << Foam::endl;
//         Foam::Info << "max_relative_error : " << max_relative_error << Foam::endl;
//         Foam::Info << "---------------------------------------------------------" << Foam::endl;
//     }

//     df_free(fp16);
//     df_free(check);
// }
#include "CSRGAMGPreconditioner.H"
void Foam::CSRGAMGPreconditioner::print_time() const {
        if (!(procondition_time > 0.0)) {
            Info << "CSRGAMGPreconditioner: not applied; timing percentages N/A" << endl;
            return;
        }
        double other_time = procondition_time - spmv_time - smooth_time - scale_time - interpolate_time - restrictField_time - prolongField_time;
        Info << "CSRGAMGPreconditioner procondition profiling -------------------------------------------" << endl;
        Info << "procondition time " << procondition_time << endl;
        Info << "spmv time : " << spmv_time << ", " << spmv_time / procondition_time * 100 << "%" << endl;
        Info << "smooth time : " << smooth_time << ", " << smooth_time / procondition_time * 100 << "%" << endl;
        Info << "scale time : " << scale_time << ", " << scale_time / procondition_time * 100 << "%" << endl;
        Info << "interpolate time : " << interpolate_time << ", " << interpolate_time / procondition_time * 100 << "%" << endl;
        Info << "scale spmv time : " << scale_spmv_time << ", " << scale_spmv_time / procondition_time * 100 << "%" << endl;
        Info << "interpolate spmv time : " << interpolate_spmv_time << ", " << interpolate_spmv_time / procondition_time * 100 << "%" << endl;
        Info << "restrictField time : " << restrictField_time << ", " << restrictField_time / procondition_time * 100 << "%" << endl;
        Info << "prolongField time : " << prolongField_time << ", " << prolongField_time / procondition_time * 100 << "%" << endl;
        Info << "other time : " << other_time << ", " << other_time / procondition_time * 100 << "%" << endl;
        Info << "----------------------------------------------------------------------------------------" << endl;
    }

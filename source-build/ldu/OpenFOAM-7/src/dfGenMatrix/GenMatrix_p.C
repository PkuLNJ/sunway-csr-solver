#include "GenFvMatrix.H"

#include "gaussLaplacianScheme.H"
#include "surfaceInterpolate.H"
#include "fvcDiv.H"
#include "fvcGrad.H"
#include "fvMatrices.H"
#include "snGradScheme.H"
#include "linear.H"
#include "orthogonalSnGrad.H"

#include "pimpleControl.H"
#include "fvOptions.H"
#include "turbulentFluidThermoModel.H"

#include "incompressibleTwoPhaseMixture.H"
#include "immiscibleIncompressibleTwoPhaseMixture.H"
#include "turbulentTransportModel.H"
#include <mpi.h>

double p_rghEqn_build_pre_time = 0., p_rghEqn_build_item1_intern_time = 0., p_rghEqn_build_item1_bound_time = 0., p_rghEqn_build_item2_time = 0.;
double start, end;

namespace Foam{

template<class Type>
tmp<fvMatrix<Type>>
GenMatrix_p(
    const GeometricField<scalar, fvsPatchField, surfaceMesh>& rAUff,
    const GeometricField<Type, fvPatchField, volMesh>& p_rgh,
    const tmp<GeometricField<double, fvsPatchField, surfaceMesh>>& phiHbyA
){
    p_rghEqn_build_pre_time = 0., p_rghEqn_build_item1_intern_time = 0., p_rghEqn_build_item1_bound_time = 0., p_rghEqn_build_item2_time = 0.;
    Info << "pEqn gaussLaplacianSchemeFvmLaplacian" << endl;
    start = MPI_Wtime();
    
    const fvMesh& mesh = p_rgh.mesh();
    tmp<fv::snGradScheme<scalar>> tsnGradScheme_(new fv::correctedSnGrad<scalar>(mesh));

    surfaceScalarField rAUf = rAUff;

    GeometricField<scalar, fvsPatchField, surfaceMesh> gammaMagSf
    (
        rAUf*mesh.magSf()
    );

    const surfaceScalarField& deltaCoeffs = tsnGradScheme_().deltaCoeffs(p_rgh);

    // -----

    tmp<volScalarField> tdivPhiHbyA = tmp<volScalarField>
    (
        new volScalarField
        (
            "div(" + phiHbyA().name() + ')',
            fvc::surfaceIntegrate(phiHbyA())
        )
    );
    
    // -----
    tmp<fvMatrix<Type>> tfvm_Lap
    (
        new fvScalarMatrix
        (
            p_rgh,
            deltaCoeffs.dimensions()*gammaMagSf.dimensions()*p_rgh.dimensions()
        )
    );

    fvScalarMatrix& fvm = tfvm_Lap.ref();
    
    end = MPI_Wtime();
    p_rghEqn_build_pre_time += end - start;
    // -----
    start = MPI_Wtime();



    // scalar* const __restrict__ prghUpperPtr = fvm.upper().begin();
    // scalar* const __restrict__ prghDiagPtr = fvm.diag().begin();
    // scalar* const __restrict__ prghLowerPtr = fvm.lower().begin();

    // const scalar* const __restrict__ prghDeltaCoeffsPtr = deltaCoeffs.primitiveField().begin();
    // const scalar* const __restrict__ prghGammaMagSfPtr = gammaMagSf.primitiveField().begin();

    // const labelUList& prghL = fvm.lduAddr().lowerAddr();
    // const labelUList& prghU = fvm.lduAddr().upperAddr();

    // const label prghNFaces = fvm.upper().size();
    // const label prghNCells = fvm.diag().size();

    // // 方法1：严格模拟原始操作顺序
    // // 第一步：只设置上三角
    // for (label prghFacei = 0; prghFacei < prghNFaces; prghFacei++)
    // {
    //     prghUpperPtr[prghFacei] = prghDeltaCoeffsPtr[prghFacei] * prghGammaMagSfPtr[prghFacei];
    // }

    // // 第二步：negSumDiag 操作（可能包含边界贡献）
    // // 先保存原始的对角线状态
    // for (label prghCelli = 0; prghCelli < prghNCells; prghCelli++)
    // {
    //     // 如果对角线已经有值（来自边界条件），先保存
    //     // 否则初始化为0
    //     prghDiagPtr[prghCelli] = 0.0;
    // }

    // // 累加内部面对角线贡献
    // for (label prghFacei = 0; prghFacei < prghNFaces; prghFacei++)
    // {
    //     prghDiagPtr[prghL[prghFacei]] += prghUpperPtr[prghFacei];
    //     prghDiagPtr[prghU[prghFacei]] += prghUpperPtr[prghFacei];
    // }

    // // 取负值
    // for (label prghCelli = 0; prghCelli < prghNCells; prghCelli++)
    // {
    //     prghDiagPtr[prghCelli] = -prghDiagPtr[prghCelli];
    // }

    // // 第三步：设置下三角（对称矩阵）
    // for (label prghFacei = 0; prghFacei < prghNFaces; prghFacei++)
    // {
    //     prghLowerPtr[prghFacei] = prghUpperPtr[prghFacei];
    // }






    fvm.upper() = deltaCoeffs.primitiveField()*gammaMagSf.primitiveField();
    fvm.negSumDiag();



    // scalar maxDiff = 0.0;
    // for (label prghFacei = 0; prghFacei < prghNFaces; prghFacei++)
    // {
    //     scalar expected = deltaCoeffs.primitiveField()[prghFacei] * gammaMagSf.primitiveField()[prghFacei];
    //     scalar actual = prghUpperPtr[prghFacei];
    //     maxDiff = max(maxDiff, fabs(expected - actual));
    // }
    // Info << "Max difference in upper: " << maxDiff << endl;
    // maxDiff = 0.0;
    // for (label prghFacei = 0; prghFacei < prghNFaces; prghFacei++)
    // {
    //     scalar expected = deltaCoeffs.primitiveField()[prghFacei] * gammaMagSf.primitiveField()[prghFacei];
    //     scalar actual = prghLowerPtr[prghFacei];
    //     maxDiff = max(maxDiff, fabs(expected - actual));
    // }
    // Info << "Max difference in lower: " << maxDiff << endl;

    // maxDiff = 0.0;
    // // 创建一个临时矩阵用于比较
    // fvScalarMatrix fvm_ref
    // (
    //     p_rgh,
    //     deltaCoeffs.dimensions()*gammaMagSf.dimensions()*p_rgh.dimensions()
    // );
    
    // // 使用原始操作
    // fvm_ref.upper() = deltaCoeffs.primitiveField()*gammaMagSf.primitiveField();
    // fvm_ref.negSumDiag();
    
    // maxDiff = 0.0;
    // for (label prghCelli = 0; prghCelli < prghNCells; prghCelli++)
    // {
    //     scalar expected = fvm_ref.diag()[prghCelli];
    //     scalar actual = prghDiagPtr[prghCelli];
    //     maxDiff = max(maxDiff, fabs(expected - actual));
    // }
    // Info << "Max difference in diag (vs reference): " << maxDiff << endl;
    

    
    end = MPI_Wtime();
    p_rghEqn_build_item1_intern_time += end - start;

    start = MPI_Wtime();

    forAll(p_rgh.boundaryField(), patchi)
    {
        const fvPatchField<Type>& pvf = p_rgh.boundaryField()[patchi];
        const fvsPatchScalarField& pGamma = gammaMagSf.boundaryField()[patchi];
        const fvsPatchScalarField& pDeltaCoeffs =
            deltaCoeffs.boundaryField()[patchi];

        if (pvf.coupled())
        {
            fvm.internalCoeffs()[patchi] =
                pGamma*pvf.gradientInternalCoeffs(pDeltaCoeffs);
            fvm.boundaryCoeffs()[patchi] =
               -pGamma*pvf.gradientBoundaryCoeffs(pDeltaCoeffs);
        }
        else
        {
            fvm.internalCoeffs()[patchi] = pGamma*pvf.gradientInternalCoeffs();
            fvm.boundaryCoeffs()[patchi] = -pGamma*pvf.gradientBoundaryCoeffs();
        }
    }

    if (mesh.fluxRequired(p_rgh.name()))
    {
        fvm.faceFluxCorrectionPtr() = new
        GeometricField<Type, fvsPatchField, surfaceMesh>
        (
            gammaMagSf*tsnGradScheme_().correction(p_rgh)
        );

        fvm.source() -=
            mesh.V()*
            fvc::div
            (
                *fvm.faceFluxCorrectionPtr()
            )().primitiveField();
    }
    else
    {
        fvm.source() -=
            mesh.V()*
            fvc::div
            (
                gammaMagSf*tsnGradScheme_().correction(p_rgh)
            )().primitiveField();
    }

    end = MPI_Wtime();
    p_rghEqn_build_item1_bound_time += end - start;
    // -------------------------------------------------------
    
    Info << "pEqn gaussConvectionFvcDiv" << endl;
    start = MPI_Wtime();

    fvm.source() += mesh.V() * tdivPhiHbyA().primitiveField();

    end = MPI_Wtime();
    p_rghEqn_build_item2_time += end - start;

    // scalar* __restrict__ sourcePtrs = fvm.source().begin();
    // const scalar* const __restrict__ divPhiHbyAPtrs = tdivPhiHbyA().primitiveField().begin();
    // const scalar* const __restrict__ meshVPtrs = mesh.V().begin();
    // const label nCellss = fvm.diag().size();

    // for (label cell = 0; cell < nCellss; cell++)
    // {
    //     sourcePtrs[cell] = 0.0;
    //     sourcePtrs[cell] += meshVPtrs[cell]*divPhiHbyAPtrs[cell];
    // }
    
    // -------------------------------------------------------
    // interFoam   pEqn.H

    // 创建参考矩阵
    // tmp<fvScalarMatrix> fvm_ref
    // (
    //     fvm::laplacian(rAUf, p_rgh) 
    //     - fvc::div(phiHbyA)
    // );

    // // 验证上三角
    // scalar maxDiff = 0.0;
    // for (label prghFacei = 0; prghFacei < prghNFaces; prghFacei++)
    // {
    //     scalar expected = fvm_ref().upper()[prghFacei];
    //     scalar actual = prghUpperPtr[prghFacei];
    //     maxDiff = max(maxDiff, fabs(expected - actual));
    // }
    // Info << "Max difference in upper: " << maxDiff << endl;

    // // 验证下三角
    // maxDiff = 0.0;
    // for (label prghFacei = 0; prghFacei < prghNFaces; prghFacei++)
    // {
    //     scalar expected = fvm_ref().lower()[prghFacei];
    //     scalar actual = prghLowerPtr[prghFacei];
    //     maxDiff = max(maxDiff, fabs(expected - actual));
    // }
    // Info << "Max difference in lower: " << maxDiff << endl;

    // // 验证对角线
    // maxDiff = 0.0;
    // for (label prghCelli = 0; prghCelli < prghNCells; prghCelli++)
    // {
    //     scalar expected = fvm_ref().diag()[prghCelli];
    //     scalar actual = prghDiagPtr[prghCelli];
    //     maxDiff = max(maxDiff, fabs(expected - actual));
    // }
    // Info << "Max difference in diag: " << maxDiff << endl;

    // // 验证源项
    // maxDiff = 0.0;
    // const scalar* const __restrict__ prghSourcePtr = fvm.source().begin();
    // const scalar* const __restrict__ prghRefSourcePtr = fvm_ref().source().begin();
    // for (label prghCelli = 0; prghCelli < prghNCells; prghCelli++)
    // {
    //     scalar expected = prghRefSourcePtr[prghCelli];
    //     scalar actual = prghSourcePtr[prghCelli];
    //     maxDiff = max(maxDiff, fabs(expected - actual));
    // }
    // Info << "Max difference in source: " << maxDiff << endl;

    // // 验证边界系数 - 内部系数
    // maxDiff = 0.0;
    // forAll(p_rgh.boundaryField(), patchi)
    // {
    //     const Field<scalar>& expectedInternal = fvm_ref().internalCoeffs()[patchi];
    //     const Field<scalar>& actualInternal = fvm.internalCoeffs()[patchi];
        
    //     forAll(expectedInternal, i)
    //     {
    //         scalar diff = fabs(expectedInternal[i] - actualInternal[i]);
    //         maxDiff = max(maxDiff, diff);
    //     }
    // }
    // Info << "Max difference in internalCoeffs: " << maxDiff << endl;

    // // 验证边界系数 - 边界系数
    // maxDiff = 0.0;
    // forAll(p_rgh.boundaryField(), patchi)
    // {
    //     const Field<scalar>& expectedBoundary = fvm_ref().boundaryCoeffs()[patchi];
    //     const Field<scalar>& actualBoundary = fvm.boundaryCoeffs()[patchi];
        
    //     forAll(expectedBoundary, i)
    //     {
    //         scalar diff = fabs(expectedBoundary[i] - actualBoundary[i]);
    //         maxDiff = max(maxDiff, diff);
    //     }
    // }
    // Info << "Max difference in boundaryCoeffs: " << maxDiff << endl;

    // // 验证通量修正（如果存在）
    // if (mesh.fluxRequired(p_rgh.name()) && fvm_ref().faceFluxCorrectionPtr())
    // {
    //     maxDiff = 0.0;
    //     const surfaceScalarField& expectedFluxCorr = *fvm_ref().faceFluxCorrectionPtr();
    //     const surfaceScalarField& actualFluxCorr = *fvm.faceFluxCorrectionPtr();
        
    //     for (label facei = 0; facei < expectedFluxCorr.size(); facei++)
    //     {
    //         scalar expected = expectedFluxCorr.primitiveField()[facei];
    //         scalar actual = actualFluxCorr.primitiveField()[facei];
    //         maxDiff = max(maxDiff, fabs(expected - actual));
    //     }
    //     Info << "Max difference in faceFluxCorrection: " << maxDiff << endl;
    // }

    // 输出统计信息
    // Info << "Matrix validation summary:" << endl;
    // Info << "  - Upper size: " << prghNFaces << endl;
    // Info << "  - Diag size: " << prghNCells << endl;
    // Info << "  - Source size: " << prghNCells << endl;
    // Info << "  - Number of patches: " << p_rgh.boundaryField().size() << endl;



    // tmp<fvScalarMatrix> tfvm
    // (
    //         tfvm_Lap
    //     // + fvm::laplacian(rAUf, p_rgh) 
    //     // - fvc::div(phiHbyA)
    // );

    Info<< "========Time Spent in pEqn build once=================="<< endl;
    Info << "p pre Time :   " << p_rghEqn_build_pre_time << endl;
    Info << "p item1 intern Time : " << p_rghEqn_build_item1_intern_time << endl;
    Info << "p item1 bound Time : " << p_rghEqn_build_item1_bound_time << endl;
    Info << "p item2 Time : " << p_rghEqn_build_item2_time << endl;
    Info<< "============================================"<<nl<< endl;

    // return tfvm;
    return tfvm_Lap;
}

}


namespace Foam {
    template tmp<fvMatrix<scalar>> GenMatrix_p<scalar>(
        const GeometricField<scalar, fvsPatchField, surfaceMesh>&,
        const GeometricField<scalar, fvPatchField, volMesh>&,
        const tmp<GeometricField<double, fvsPatchField, surfaceMesh>>&
    );
}
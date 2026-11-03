{
    double start, end;
    double start0, end0;
    double start1, end1;

    start0 = MPI_Wtime();

    start = MPI_Wtime();

    if (correctPhi)
    {
        rAU.ref() = 1.0/UEqn.A();
    }
    else
    {
        rAU = 1.0/UEqn.A();
    }

    surfaceScalarField rAUf("rAUf", fvc::interpolate(rAU()));
    volVectorField HbyA(constrainHbyA(rAU()*UEqn.H(), U, p_rgh));
    surfaceScalarField phiHbyA
    (
        "phiHbyA",
        fvc::flux(HbyA)
      + MRF.zeroFilter(fvc::interpolate(rho*rAU())*fvc::ddtCorr(U, phi, Uf))
    );
    MRF.makeRelative(phiHbyA);

    end = MPI_Wtime();
    p_pre_time += end - start;

    start = MPI_Wtime();

    if (p_rgh.needReference())
    {
        fvc::makeRelative(phiHbyA, U);
        adjustPhi(phiHbyA, U, p_rgh);
        fvc::makeAbsolute(phiHbyA, U);
    }

    surfaceScalarField phig
    (
        (
            mixture.surfaceTensionForce()
          - ghf*fvc::snGrad(rhok)
        )*rAUf*mesh.magSf()
    );

    phiHbyA += phig;

    // Update the pressure BCs to ensure flux consistency
    constrainPressure(p_rgh, U, phiHbyA, rAUf, MRF);

    end = MPI_Wtime();
    p_build_time += end - start;

    start = MPI_Wtime();

    while (pimple.correctNonOrthogonal())
    {
        start1 = MPI_Wtime();
        
        fvScalarMatrix p_rghEqn
        (
            fvm::laplacian(rAUf, p_rgh) == fvc::div(phiHbyA)
        );

        p_rghEqn.setReference(pRefCell, getRefCellValue(p_rgh, pRefCell));
        
        // csrMatrix csr(p_rghEqn);

        end1 = MPI_Wtime();
        p_rghEqn_build_time += end1 - start1;

        start1 = MPI_Wtime();

        //auto& psi = const_cast<GeometricField<scalar, fvPatchField, volMesh>&>(p_rghEqn.psi());
        //auto& source = p_rghEqn.source();
        //auto& internalCoeffs = p_rghEqn.internalCoeffs();
        //auto& boundaryCoeffs = p_rghEqn.boundaryCoeffs();
        //csr.solve(psi, source, internalCoeffs, boundaryCoeffs);

        p_rghEqn.solve();

        end1 = MPI_Wtime();
        p_rghEqn_solve_time += end1 - start1;

        start1 = MPI_Wtime();

        if (pimple.finalNonOrthogonalIter())
        {
            phi = phiHbyA - p_rghEqn.flux();

            p_rgh.relax();

            U = HbyA + rAU()*fvc::reconstruct((phig - p_rghEqn.flux())/rAUf);
            U.correctBoundaryConditions();
            fvOptions.correct(U);
        }
        end1 = MPI_Wtime();
        p_rghEqn_post_time += end1 - start1;
    }

    end = MPI_Wtime();
    p_solve_time += end - start;

    start = MPI_Wtime();

    #include "continuityErrs.H"

    // Correct Uf if the mesh is moving
    fvc::correctUf(Uf, U, phi);

    // Make the fluxes relative to the mesh motion
    fvc::makeRelative(phi, U);

    p == p_rgh + rhok*gh;

    if (p_rgh.needReference())
    {
        p += dimensionedScalar
        (
            "p",
            p.dimensions(),
            pRefValue - getRefCellValue(p, pRefCell)
        );
        p_rgh = p - rhok*gh;
    }

    if (!correctPhi)
    {
        rAU.clear();
    }

    end = MPI_Wtime();
    p_post_time += end - start;

    end0 = MPI_Wtime();
    p_total_time += end0 - start0;
}

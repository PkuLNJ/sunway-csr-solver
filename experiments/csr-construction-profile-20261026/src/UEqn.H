    U_total_start = MPI_Wtime();

    U_build_start = MPI_Wtime();
    U_buildMRF_start = MPI_Wtime();

    MRF.correctBoundaryVelocity(U);

    U_buildMRF_end = MPI_Wtime();
    U_buildMRF_time += U_buildMRF_end - U_buildMRF_start;

    U_buildEqn_start = MPI_Wtime();

    fvVectorMatrix UEqn
    (
        lnjConstruct::timed(8,[&](){return fvm::ddt(rho, U);}) + lnjConstruct::timed(9,[&](){return fvm::div(rhoPhi, U);})
      + lnjConstruct::timed(10,[&](){return MRF.DDt(rho, U);})
      + lnjConstruct::timed(11,[&](){return turbulence->divDevRhoReff(rho, U);})
     ==
        lnjConstruct::timed(12,[&](){return fvOptions(rho, U);})
    );

    U_buildEqn_end = MPI_Wtime();
    U_buildEqn_time += U_buildEqn_end - U_buildEqn_start;

    // Info << "build right "<< endl;

    U_build_end = MPI_Wtime();
    U_build_time += U_build_end - U_build_start;

    U_relax_start = MPI_Wtime();

    UEqn.relax();

    // Info << "relax right "<< endl;

    U_relax_end = MPI_Wtime();
    U_relax_time += U_relax_end - U_relax_start;

    U_constrain_start = MPI_Wtime();

    fvOptions.constrain(UEqn);

    // Info << "constrain right "<< endl;

    U_constrain_end = MPI_Wtime();
    U_constrain_time += U_constrain_end - U_constrain_start;

    U_solve_start = MPI_Wtime();

    if (pimple.momentumPredictor())
    {
        solve
        (
            UEqn
         ==
            lnjConstruct::timed(13,[&](){return fvc::reconstruct
            (
                (
                    mixture.surfaceTensionForce()
                  - ghf*fvc::snGrad(rhok)
                  - fvc::snGrad(p_rgh)
                ) * mesh.magSf()
            );})
        );

        fvOptions.correct(U);
    }
    
    // Info << "solve right "<< endl;

    U_solve_end = MPI_Wtime();
    U_solve_time += U_solve_end - U_solve_start;

    U_total_end = MPI_Wtime();
    U_total_time += U_total_end - U_total_start;


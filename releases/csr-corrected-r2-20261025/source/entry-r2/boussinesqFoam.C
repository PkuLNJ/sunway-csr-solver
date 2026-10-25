/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     | Website:  https://openfoam.org
    \\  /    A nd           | Copyright (C) 2011-2019 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

Application
    interFoam

Description
    Solver for 2 incompressible, isothermal immiscible fluids using a VOF
    (volume of fluid) phase-fraction based interface capturing approach,
    with optional mesh motion and mesh topology changes including adaptive
    re-meshing.

\*---------------------------------------------------------------------------*/

#include "fvCFD.H"
#include "AmrCapture.H"
#include "dynamicFvMesh.H"
#include "CMULES.H"
#include "EulerDdtScheme.H"
#include "localEulerDdtScheme.H"
#include "CrankNicolsonDdtScheme.H"
#include "subCycle.H"
#include "immiscibleIncompressibleTwoPhaseMixture.H"
#include "turbulentTransportModel.H"
#include "pimpleControl.H"
#include "fvOptions.H"
#include "CorrectPhi.H"
#include "fvcSmooth.H"

#include "clockTime.H"
#include <omp.h>
#include <mpi.h>

// DF_MATRIX
// #include "env.H"
// #include "csrPattern.H"
// #include "BCSRPattern.H"
// // #include "dfMatrix.H"
// // #include "dfLduMatrix.H
// // #include "LDUMatrix.H"
#include "csrMatrix.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

int main(int argc, char *argv[])
{
    #include "postProcess.H"

    #include "setRootCaseLists.H"
    #include "createTime.H"
    #include "createDynamicFvMesh.H"
    amrCapture::init();
    #include "initContinuityErrs.H"
    #include "createDyMControls.H"
    #include "createFields.H"
    amrCapture::registered("initialized_before_startup", mesh);
    amrCapture::mesh("initial_mesh", mesh);
    #include "createAlphaFluxes.H"
    #include "initCorrectPhi.H"
    amrCapture::registered("after_startup_before_Uf", mesh);
    #include "createUfIfPresent.H"
    amrCapture::registered("after_initial_Uf", mesh);

    turbulence->validate();

    if (!LTS)
    {
        #include "CourantNo.H"
        #include "setInitialDeltaT.H"
    }

    // * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
    Info<< "\nStarting time loop\n" << endl;

    bool amrCaptureFirstParameters = true;

    while (runTime.run())
    {
        double time_readDyMControls=0.;
        double time_setDeltaT=0.;
        double time_monitor_loop=0.;
        double time_firstPimple=0.;
        double time_monitor_alpha=0.;
        double time_mixture_correct=0.;
        double time_updateParameters=0.;
        double time_TEqn=0.;
        double time_SEqn=0.;
        double time_monitor_U=0.;
        double time_monitor_p=0.;
        double time_turbulence_correct=0.;
        double start, end;
        double start0, end0;
        double start1, end1;
        
        double U_total_start, U_total_end;
        double U_build_start, U_build_end, U_build_time = 0.;
        double U_buildMRF_start, U_buildMRF_end, U_buildMRF_time = 0.;
        double U_buildEqn_start, U_buildEqn_end, U_buildEqn_time = 0.;
        double U_relax_start, U_relax_end, U_relax_time = 0.;
        double U_constrain_start, U_constrain_end, U_constrain_time = 0.;
        double U_solve_start, U_solve_end, U_solve_time = 0.;
        double U_total_time = 0.;

        double p_pre_time = 0.;
        double p_build_time = 0.;
        double p_solve_time = 0.;
        double p_post_time = 0.;
        double p_total_time = 0.;
        double p_rghEqn_build_time = 0., p_rghEqn_solve_time = 0., p_rghEqn_post_time = 0.;

        double T_build_time = 0., T_solve_time = 0., T_total_time = 0.;
        double S_build_time = 0., S_solve_time = 0., S_total_time = 0.;

        double time_mesh_update = 0., time_topo_change = 0., time_gh = 0., time_MRF_update = 0.;
        double time_first_correctPhi = 0., time_first_mixcorrect = 0., time_meshCourantNo = 0.;

        start0 = MPI_Wtime();
        
        start = MPI_Wtime();
        #include "readDyMControls.H"
        end = MPI_Wtime();
        time_readDyMControls += end - start;

        start = MPI_Wtime();
        if (LTS)
        {
            #include "setRDeltaT.H"
        }
        else
        {
            #include "CourantNo.H"
            #include "alphaCourantNo.H"
            #include "setDeltaT.H"
        }
        end = MPI_Wtime();
        time_setDeltaT += end - start;

        runTime++;
        Info << nl << "Time = " << runTime.timeName() << nl << endl;

        // --- Pressure-velocity PIMPLE corrector loop
        while (pimple.loop())
        {
            start = MPI_Wtime();
            if (pimple.firstPimpleIter() || moveMeshOuterCorrectors)
            {
                start1 = MPI_Wtime();
                const bool amrProbeFirst = !amrCapture::hasRefined();
                if (amrProbeFirst) amrCapture::registered("entry_before_mesh_update", mesh);
                mesh.update();
                if (amrProbeFirst)
                {
                    amrCapture::registered("entry_after_mesh_update", mesh);
                    if (mesh.topoChanging()) amrCapture::topologyChanged();
                }
                end1 = MPI_Wtime();
                time_mesh_update += end1 - start1;

                if (mesh.changing())
                {
                    // Do not apply previous time-step mesh compression flux
                    // if the mesh topology changed
                    start1 = MPI_Wtime();

                    if (mesh.topoChanging())
                    {
                        talphaPhi1Corr0.clear();
                    }

                    end1 = MPI_Wtime();
                    time_topo_change += end1 - start1;

                    start1 = MPI_Wtime();

                    gh = (g & mesh.C()) - ghRef;
                    ghf = (g & mesh.Cf()) - ghRef;

                    end1 = MPI_Wtime();
                    time_gh += end1 - start1;

                    start1 = MPI_Wtime();

                    MRF.update();

                    end1 = MPI_Wtime();
                    time_MRF_update += end1 - start1;

                    start1 = MPI_Wtime();

                    if (correctPhi)
                    {
                        // Calculate absolute flux
                        // from the mapped surface velocity
                        if (amrProbeFirst) amrCapture::registered("before_phi_rebuild", mesh);
                        phi = mesh.Sf() & Uf();
                        if (amrProbeFirst)
                        {
                            amrCapture::field("phi_rebuild", mesh.Sf());
                            amrCapture::field("phi_rebuild", Uf());
                            amrCapture::field("phi_rebuild", phi);
                        }

                        #include "correctPhi.H"
                        if (amrProbeFirst) amrCapture::registered("after_amr_correctPhi", mesh);
                        Info<< "correctPhi.H" << endl;

                        // Make the flux relative to the mesh motion
                        fvc::makeRelative(phi, U);
                    }

                    end1 = MPI_Wtime();
                    time_first_correctPhi += end1 - start1;

                    start1 = MPI_Wtime();

                    mixture.correct();

                    end1 = MPI_Wtime();
                    time_first_mixcorrect += end1 - start1;

                    start1 = MPI_Wtime();

                    if (checkMeshCourantNo)
                    {
                        #include "meshCourantNo.H"
                    }
                    end1 = MPI_Wtime();
                    time_meshCourantNo += end1 - start1;
                }
            }
            end = MPI_Wtime();
            time_firstPimple += end - start;

            Info<< "alpha.H" << endl;
            start = MPI_Wtime();
            #include "alphaControls.H"
            #include "alphaEqnSubCycle.H"
            end = MPI_Wtime();
            time_monitor_alpha += end - start;

            Info<< "mixture.correct" << endl;
            start = MPI_Wtime();
            mixture.correct();
            end = MPI_Wtime();
            time_mixture_correct += end - start;

            Info<< "updateParameters.H" << endl;
            start = MPI_Wtime();
            const bool captureFirstParameters = amrCapture::enabled() && amrCaptureFirstParameters;
            if (captureFirstParameters)
            {
                amrCapture::field("first_updateParameters.before", TT);
                amrCapture::field("first_updateParameters.before", S);
            }
            #include "updateParameters.H"
            if (captureFirstParameters)
            {
                amrCapture::field("first_updateParameters.after", Cp);
                amrCapture::field("first_updateParameters.after", kk);
                amrCapture::field("first_updateParameters.after", rhok);
            }
            amrCaptureFirstParameters = false;
            end = MPI_Wtime();
            time_updateParameters += end - start;

            Info<< "TEqn.H" << endl;
            start = MPI_Wtime();
            #include "TEqn.H"
            end = MPI_Wtime();
            time_TEqn += end - start;

            Info<< "SEqn.H" << endl;
            start = MPI_Wtime();
            #include "SEqn.H"
            end = MPI_Wtime();
            time_SEqn += end - start;

            Info<< "UEqn.H" << endl;
            start = MPI_Wtime();
            #include "UEqn.H"
            end = MPI_Wtime();
            time_monitor_U += end - start;

            // --- Pressure corrector loop
            while (pimple.correct())
            {
                Info<< "pEqn.H" << endl;
                start = MPI_Wtime();
                #include "pEqn.H"
                end = MPI_Wtime();
                time_monitor_p += end - start;
            }

            if (pimple.turbCorr())
            {
                Info<< "turbulence.correct" << endl;
                start = MPI_Wtime();
                turbulence->correct();
                end = MPI_Wtime();
                time_turbulence_correct += end - start;    
            }
        }

        end0 = MPI_Wtime();
        time_monitor_loop += end0 - start0;

        if (amrCapture::enabled())
        {
            amrCapture::registered("end_first_step", mesh);
            amrCapture::finish();
            Info<< "LNJ_AMR_CAPTURE_ONE_STEP_COMPLETE" << endl;
            return 0;
        }
        runTime.write();

        Info<< "========Time Spent in diffenet parts========"<< endl;
        Info<< "ExecutionTime = " << runTime.elapsedCpuTime() << " s"
            << "  ClockTime = " << runTime.elapsedClockTime() << " s" << endl;
        Info<< "Loop Time                  = " << time_monitor_loop << " s" << endl;
        Info<< "readDyMControls            = " << time_readDyMControls << " s" << endl;
        Info<< "setDeltaT                  = " << time_setDeltaT << " s" << endl;
        Info<< "firstPimple                = " << time_firstPimple << " s " << endl;
        Info<< "alpha Equations            = " << time_monitor_alpha << " s " << endl;
        Info<< "mixture_correct            = " << time_mixture_correct << " s " << endl;
        Info<< "updateParameters           = " << time_updateParameters << " s " << endl;
        Info<< "T Equations                = " << time_TEqn << " s " << endl;
        Info<< "S Equations                = " << time_SEqn << " s " << endl;
        Info<< "U Equations                = " << time_monitor_U << " s " << endl;
        Info<< "    U build                = " << U_build_time << " s " << endl;
        Info<< "    U solve                = " << U_solve_time << " s "<< endl;
        Info<< "p Equations                = " << time_monitor_p << " s " << endl;
        Info<< "    p solve                = " << p_rghEqn_solve_time << " s " << endl;
        Info<< "    p build (p+p_rgh)      = " << p_rghEqn_build_time+p_build_time << endl;
        Info<< "turbulence_correct         = " << time_turbulence_correct << " s " << endl;
        Info<< "============================================"<<nl<< endl;

        Info<< "========Time Spent in first pimple========"<< endl;
        Info << "first mesh update Time :  " << time_mesh_update << endl;
        Info << "first topo change Time :  " << time_topo_change << endl;
        Info << "first gh compute Time :   " << time_gh << endl;
        Info << "first MRF update Time :   " << time_MRF_update << endl;
        Info << "first correctPhi Time :   " << time_first_correctPhi << endl;
        Info << "first mixcorrect Time :   " << time_first_mixcorrect << endl;
        Info << "first mesh Courant Time : " << time_meshCourantNo << endl;
        Info<< "============================================"<<nl<< endl;

        Info<< "========Time Spent in UEqn================="<< endl;
        Info << "U build Time : " << U_build_time << endl;
        Info<< "========Time Spent in build========"<< endl;
        Info << "U buildMRF Time (in build) :  " << U_buildMRF_time << " s " << endl;
        Info << "U buildEqn Time (in build) :  " << U_buildEqn_time << " s " << endl;
        Info<< "==================================="<< endl;
        Info << "U relax Time :     " << U_relax_time << endl;
        Info << "U constrain Time : " << U_constrain_time << endl;
        Info << "U solve Time :     " << U_solve_time << endl;
        Info << "U total Time :     " << U_total_time << endl;
        Info<< "============================================"<<nl<< endl;

        Info<< "========Time Spent in pEqn=================="<< endl;
        Info << "p pre Time :   " << p_pre_time << endl;
        Info << "p build Time : " << p_build_time << endl;
        Info << "p solve Time : " << p_solve_time << endl;
        Info<< "========Time Spent in solve========"<< endl;
        Info << "p_rghEqn build Time (in solve) :  " << p_rghEqn_build_time << " s " << endl;
        Info << "p_rghEqn solve Time (in solve) :  " << p_rghEqn_solve_time << " s " << endl;
        Info << "p_rghEqn post Time (in solve) :   " << p_rghEqn_post_time << " s " << endl;
        Info<< "==================================="<< endl;
        Info << "p post Time :  " << p_post_time << endl;
        Info << "p total Time : " << p_total_time << endl;
        Info<< "============================================"<<nl<< endl;

        Info<< "========Time Spent in T and S Equations====="<< endl;
        Info << "T total Time : " << T_total_time << " s" << endl;
        Info << "T build Time : " << T_build_time << " s " << endl;
        Info << "T solve Time : " << T_solve_time << " s " << endl;
        Info << "S total Time : " << S_total_time << " s" << endl;
        Info << "S build Time : " << S_build_time << " s " << endl;
        Info << "S solve Time : " << S_solve_time << " s " << endl;
        Info<< "============================================"<<nl<< endl;
    }

    Info<< "End\n" << endl;

    return 0;
}


// ************************************************************************* //

/* -------------------------------------------------------------------------- *
 *                                   OpenMM                                   *
 * -------------------------------------------------------------------------- *
 * This is part of the OpenMM molecular simulation toolkit originating from   *
 * Simbios, the NIH National Center for Physics-Based Simulation of           *
 * Biological Structures at Stanford, funded under the NIH Roadmap for        *
 * Medical Research, grant U54 GM072970. See https://simtk.org.               *
 *                                                                            *
 * Portions copyright (c) 2008-2009 Stanford University and the Authors.      *
 * Authors: Peter Eastman                                                     *
 * Contributors:                                                              *
 *                                                                            *
 * Permission is hereby granted, free of charge, to any person obtaining a    *
 * copy of this software and associated documentation files (the "Software"), *
 * to deal in the Software without restriction, including without limitation  *
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,   *
 * and/or sell copies of the Software, and to permit persons to whom the      *
 * Software is furnished to do so, subject to the following conditions:       *
 *                                                                            *
 * The above copyright notice and this permission notice shall be included in *
 * all copies or substantial portions of the Software.                        *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR *
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   *
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    *
 * THE AUTHORS, CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,    *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR      *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE  *
 * USE OR OTHER DEALINGS IN THE SOFTWARE.                                     *
 * -------------------------------------------------------------------------- */

/**
 * This tests the Ewald summation method OpenCL implementation of NonbondedForce.
 */

#include "../../../tests/AssertionUtilities.h"
#include "openmm/Context.h"
#include "OpenCLPlatform.h"
#include "ReferencePlatform.h"
#include "openmm/NonbondedForce.h"
#include "openmm/System.h"
#include "openmm/LangevinIntegrator.h"
#include "openmm/VerletIntegrator.h"
#include "openmm/internal/ContextImpl.h"
#include "../src/SimTKUtilities/SimTKOpenMMRealType.h"
#include "../src/sfmt/SFMT.h"
#include <iostream>
#include <vector>

using namespace OpenMM;
using namespace std;

const double TOL = 1e-5;

void testEwaldPME() {

//      Use amorphous NaCl system for the tests

    const int numParticles 	= 894;
    const double cutoff 	= 1.2;
    const double boxSize 	= 3.00646;
    double tol 				= 1e-5;

    OpenCLPlatform cl;
    ReferencePlatform reference;
    System system;
    VerletIntegrator integrator(0.01);
    NonbondedForce* nonbonded = new NonbondedForce();
    nonbonded->setNonbondedMethod(NonbondedForce::Ewald);
    nonbonded->setCutoffDistance(cutoff);
    nonbonded->setEwaldErrorTolerance(tol);

    for (int i = 0; i < numParticles/2; i++)
        system.addParticle(22.99);
    for (int i = 0; i < numParticles/2; i++)
        system.addParticle(35.45);
    for (int i = 0; i < numParticles/2; i++)
        nonbonded->addParticle(1.0, 1.0,0.0);
    for (int i = 0; i < numParticles/2; i++)
        nonbonded->addParticle(-1.0, 1.0,0.0);
    system.setPeriodicBoxVectors(Vec3(boxSize, 0, 0), Vec3(0, boxSize, 0), Vec3(0, 0, boxSize));
    system.addForce(nonbonded);

    vector<Vec3> positions(numParticles);
    #include "nacl_amorph.dat"

//    (1)  Check whether the Reference and OpenCL platforms agree when using Ewald Method

    Context clContext(system, integrator, cl);
    Context referenceContext(system, integrator, reference);
    clContext.setPositions(positions);
    referenceContext.setPositions(positions);
    State clState = clContext.getState(State::Forces | State::Energy);
    State referenceState = referenceContext.getState(State::Forces | State::Energy);
    tol = 1e-2;
    for (int i = 0; i < numParticles; i++) {
        ASSERT_EQUAL_VEC(referenceState.getForces()[i], clState.getForces()[i], tol);
    }
    tol = 1e-5;
    ASSERT_EQUAL_TOL(referenceState.getPotentialEnergy(), clState.getPotentialEnergy(), tol);

//    (2) Check whether Ewald method in OpenCL is self-consistent

    double norm = 0.0;
    for (int i = 0; i < numParticles; ++i) {
        Vec3 f = clState.getForces()[i];
        norm += f[0]*f[0] + f[1]*f[1] + f[2]*f[2];
    }

    norm = std::sqrt(norm);
    const double delta = 1e-3;
    double step = delta/norm;
    for (int i = 0; i < numParticles; ++i) {
        Vec3 p = positions[i];
        Vec3 f = clState.getForces()[i];
        positions[i] = Vec3(p[0]-f[0]*step, p[1]-f[1]*step, p[2]-f[2]*step);
    }
    Context clContext2(system, integrator, cl);
    clContext2.setPositions(positions);

    tol = 1e-3;
    State clState2 = clContext2.getState(State::Energy);
    ASSERT_EQUAL_TOL(norm, (clState2.getPotentialEnergy()-clState.getPotentialEnergy())/delta, tol)

//    (3)  Check whether the Reference and OpenCL platforms agree when using PME

    nonbonded->setNonbondedMethod(NonbondedForce::PME);
    clContext.reinitialize();
    referenceContext.reinitialize();
    clContext.setPositions(positions);
    referenceContext.setPositions(positions);
    clState = clContext.getState(State::Forces | State::Energy);
    referenceState = referenceContext.getState(State::Forces | State::Energy);
    tol = 1e-2;
    for (int i = 0; i < numParticles; i++) {
        ASSERT_EQUAL_VEC(referenceState.getForces()[i], clState.getForces()[i], tol);
    }
    tol = 1e-5;
    ASSERT_EQUAL_TOL(referenceState.getPotentialEnergy(), clState.getPotentialEnergy(), tol);

//    (4) Check whether PME method in OpenCL is self-consistent

    norm = 0.0;
    for (int i = 0; i < numParticles; ++i) {
        Vec3 f = clState.getForces()[i];
        norm += f[0]*f[0] + f[1]*f[1] + f[2]*f[2];
    }

    norm = std::sqrt(norm);
    step = delta/norm;
    for (int i = 0; i < numParticles; ++i) {
        Vec3 p = positions[i];
        Vec3 f = clState.getForces()[i];
        positions[i] = Vec3(p[0]-f[0]*step, p[1]-f[1]*step, p[2]-f[2]*step);
    }
    Context clContext3(system, integrator, cl);
    clContext3.setPositions(positions);

    tol = 1e-3;
    State clState3 = clContext3.getState(State::Energy);
    ASSERT_EQUAL_TOL(norm, (clState3.getPotentialEnergy()-clState.getPotentialEnergy())/delta, tol)
}

void testEwald2Ions() {
    OpenCLPlatform platform;
    System system;
    system.addParticle(1.0);
    system.addParticle(1.0);
    VerletIntegrator integrator(0.01);
    NonbondedForce* nonbonded = new NonbondedForce();
    nonbonded->addParticle(1.0, 1, 0);
    nonbonded->addParticle(-1.0, 1, 0);
    nonbonded->setNonbondedMethod(NonbondedForce::Ewald);
    const double cutoff = 2.0;
    nonbonded->setCutoffDistance(cutoff);
    nonbonded->setEwaldErrorTolerance(TOL);
    system.setPeriodicBoxVectors(Vec3(6, 0, 0), Vec3(0, 6, 0), Vec3(0, 0, 6));
    system.addForce(nonbonded);
    Context context(system, integrator, platform);
    vector<Vec3> positions(2);
    positions[0] = Vec3(3.048000,2.764000,3.156000);
    positions[1] = Vec3(2.809000,2.888000,2.571000);
    context.setPositions(positions);
    State state = context.getState(State::Forces | State::Energy);
    const vector<Vec3>& forces = state.getForces();

    ASSERT_EQUAL_VEC(Vec3(-123.711,  64.1877, -302.716), forces[0], 10*TOL);
    ASSERT_EQUAL_VEC(Vec3( 123.711, -64.1877,  302.716), forces[1], 10*TOL);
    ASSERT_EQUAL_TOL(-217.276, state.getPotentialEnergy(), 0.01/*10*TOL*/);
}

void testErrorTolerance(NonbondedForce::NonbondedMethod method) {
    // Create a cloud of random point charges.

    const int numParticles = 51;
    const double boxWidth = 5.0;
    System system;
    system.setPeriodicBoxVectors(Vec3(boxWidth, 0, 0), Vec3(0, boxWidth, 0), Vec3(0, 0, boxWidth));
    NonbondedForce* force = new NonbondedForce();
    system.addForce(force);
    vector<Vec3> positions(numParticles);
    init_gen_rand(0);
    for (int i = 0; i < numParticles; i++) {
        system.addParticle(1.0);
        force->addParticle(-1.0+i*2.0/(numParticles-1), 1.0, 0.0);
        positions[i] = Vec3(boxWidth*genrand_real2(), boxWidth*genrand_real2(), boxWidth*genrand_real2());
    }
    force->setNonbondedMethod(method);
    OpenCLPlatform platform;

    // For various values of the cutoff and error tolerance, see if the actual error is reasonable.

    for (double cutoff = 1.0; cutoff < boxWidth/2; cutoff += 0.2) {
        force->setCutoffDistance(cutoff);
        vector<Vec3> refForces;
        double norm = 0.0;
        for (double tol = 5e-5; tol < 1e-3; tol *= 2.0) {
            force->setEwaldErrorTolerance(tol);
            VerletIntegrator integrator(0.01);
            Context context(system, integrator, platform);
            context.setPositions(positions);
            State state = context.getState(State::Forces);
            if (refForces.size() == 0) {
                refForces = state.getForces();
                for (int i = 0; i < numParticles; i++)
                    norm += refForces[i].dot(refForces[i]);
                norm = sqrt(norm);
            }
            else {
                double diff = 0.0;
                for (int i = 0; i < numParticles; i++) {
                    Vec3 delta = refForces[i]-state.getForces()[i];
                    diff += delta.dot(delta);
                }
                diff = sqrt(diff)/norm;
                ASSERT(diff < 5*tol);
            }
        }
    }
}

int main() {
    try {
     testEwaldPME();
//     testEwald2Ions();
     testErrorTolerance(NonbondedForce::Ewald);
     testErrorTolerance(NonbondedForce::PME);
    }
    catch(const exception& e) {
        cout << "exception: " << e.what() << endl;
        return 1;
    }
    cout << "Done" << endl;
    return 0;
}


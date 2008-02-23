/* -------------------------------------------------------------------------- *
 *                      SimTK Core: SimTK Simbody(tm)                         *
 * -------------------------------------------------------------------------- *
 * This is part of the SimTK Core biosimulation toolkit originating from      *
 * Simbios, the NIH National Center for Physics-Based Simulation of           *
 * Biological Structures at Stanford, funded under the NIH Roadmap for        *
 * Medical Research, grant U54 GM072970. See https://simtk.org.               *
 *                                                                            *
 * Portions copyright (c) 2008 Stanford University and the Authors.           *
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

#include "SimTKsimbody.h"
#include "../src/ConstraintImpl.h"

using namespace SimTK;
using namespace std;

const int NUM_BODIES = 10;
const Real BOND_LENGTH = 0.5;
const Real TOL = 1e-10;

#define ASSERT(cond) {SimTK_ASSERT_ALWAYS(cond, "Assertion failed");}

template <class T>
void assertEqual(T val1, T val2) {
    ASSERT(abs(val1-val2) < TOL);
}

template <int N>
void assertEqual(Vec<N> val1, Vec<N> val2) {
    for (int i = 0; i < N; ++i)
        ASSERT(abs(val1[i]-val2[i]) < TOL);
}

template<>
void assertEqual(SpatialVec val1, SpatialVec val2) {
    assertEqual(val1[0], val2[0]);
    assertEqual(val1[1], val2[1]);
}

/**
 * Create a system consisting of a chain of bodies.
 */

MultibodySystem& createSystem() {
    MultibodySystem* system = new MultibodySystem();
    SimbodyMatterSubsystem matter(*system);
    GeneralForceSubsystem forces(*system);
    Body::Rigid body(MassProperties(1.0, Vec3(0), Inertia(1)));
    for (int i = 0; i < NUM_BODIES; ++i) {
        MobilizedBody& parent = matter.updMobilizedBody(MobilizedBodyIndex(matter.getNBodies()-1));
        MobilizedBody::Gimbal b(parent, Transform(Vec3(0)), body, Transform(Vec3(BOND_LENGTH, 0, 0)));
    }
    return *system;
}

/**
 * Create a random state for the system.
 */

void createState(MultibodySystem& system, State& state) {
    system.realizeTopology();
    state = system.getDefaultState();
    Random::Uniform random;
    for (int i = 0; i < state.getNY(); ++i)
        state.updY()[i] = random.getValue();
    system.realize(state, Stage::Velocity);
    system.project(state, TOL, Vector(state.getNY(), 1), Vector(state.getNYErr(), 1), Vector(state.getNY()));
    system.realize(state, Stage::Acceleration);
}

void testBallConstraint() {
    
    State state;
    MultibodySystem& system = createSystem();
    SimbodyMatterSubsystem& matter = system.updMatterSubsystem();
    MobilizedBody& first = matter.updMobilizedBody(MobilizedBodyIndex(1));
    MobilizedBody& last = matter.updMobilizedBody(MobilizedBodyIndex(NUM_BODIES));
    Constraint::Ball constraint(first, last);
    createState(system, state);
    Vec3 r1 = first.getBodyOriginLocation(state);
    Vec3 r2 = last.getBodyOriginLocation(state);
    Vec3 dr = r1-r2;
    Vec3 v1 = first.getBodyOriginVelocity(state);
    Vec3 v2 = last.getBodyOriginVelocity(state);
    Vec3 dv = v1-v2;
    Vec3 a1 = first.getBodyOriginAcceleration(state);
    Vec3 a2 = last.getBodyOriginAcceleration(state);
    Vec3 da = a1-a2;
    assertEqual(dr.norm(), 0.0);
    assertEqual(dr, constraint.getPositionErrors(state));
    assertEqual(dv.norm(), 0.0);
    assertEqual(dv, constraint.getVelocityErrors(state));
    assertEqual(da.norm(), 0.0);
    assertEqual(da, constraint.getAccelerationErrors(state));
    delete &system;
}

void testConstantAngleConstraint() {
    
    State state;
    MultibodySystem& system = createSystem();
    SimbodyMatterSubsystem& matter = system.updMatterSubsystem();
    MobilizedBody& first = matter.updMobilizedBody(MobilizedBodyIndex(1));
    MobilizedBody& last = matter.updMobilizedBody(MobilizedBodyIndex(NUM_BODIES));
    Constraint::ConstantAngle constraint(first, UnitVec3(1, 0, 0), last, UnitVec3(0, 1, 0), 1.1);
    createState(system, state);
    Vec3 dir1 = first.getBodyRotation(state)*Vec3(1, 0, 0);
    Vec3 dir2 = last.getBodyRotation(state)*Vec3(0, 1, 0);
    Vec3 perpDir = dir1%dir2;
    Vec3 v1 = first.getBodyAngularVelocity(state);
    Vec3 v2 = last.getBodyAngularVelocity(state);
    assertEqual(dot(dir1, dir2), cos(1.1));
    assertEqual(dot(dir1, dir2)-cos(1.1), constraint.getPositionError(state));
    assertEqual(dot(v1-v2, perpDir), 0.0);
    assertEqual(dot(v1-v2, perpDir), constraint.getVelocityError(state));
    delete &system;
}

void testConstantOrientationConstraint() {
    
    State state;
    MultibodySystem& system = createSystem();
    SimbodyMatterSubsystem& matter = system.updMatterSubsystem();
    MobilizedBody& first = matter.updMobilizedBody(MobilizedBodyIndex(1));
    MobilizedBody& last = matter.updMobilizedBody(MobilizedBodyIndex(NUM_BODIES));
    Rotation r1(0.2, CoordinateAxis::XCoordinateAxis());
    Rotation r2(Pi/2, CoordinateAxis::YCoordinateAxis());
    Constraint::ConstantOrientation constraint(first, r1, last, r2);
    createState(system, state);
    Rotation rot1 = first.getBodyRotation(state);
    Rotation rot2 = last.getBodyRotation(state);
    Vec3 v1 = first.getBodyAngularVelocity(state);
    Vec3 v2 = last.getBodyAngularVelocity(state);
    Vec3 a1 = first.getBodyAngularAcceleration(state);
    Vec3 a2 = last.getBodyAngularAcceleration(state);
    
    // Extra constraints are required for assembly.  Without them, this constraint only guarantees
    // that the second body's X/Y/Z axis is perpendicular to the first body's Y/Z/X axis.
    
    assertEqual(dot(rot1*r1*Vec3(1, 0, 0), rot2*r2*Vec3(0, 1, 0)), 0.0);
    assertEqual(dot(rot1*r1*Vec3(0, 1, 0), rot2*r2*Vec3(0, 0, 1)), 0.0);
    assertEqual(dot(rot1*r1*Vec3(0, 0, 1), rot2*r2*Vec3(1, 0, 0)), 0.0);
    assertEqual(v1, v2);
    assertEqual(a1, a2);
    delete &system;
}

void testConstantSpeedConstraint() {
    
    State state;
    MultibodySystem& system = createSystem();
    SimbodyMatterSubsystem& matter = system.updMatterSubsystem();
    MobilizedBody& first = matter.updMobilizedBody(MobilizedBodyIndex(1));
    Constraint::ConstantSpeed constraint(first, MobilizerUIndex(1), 0.8);
    createState(system, state);
    assertEqual(first.getOneU(state, 1), 0.8);
    assertEqual(first.getOneU(state, 1)-0.8, constraint.getVelocityError(state));
    assertEqual(first.getOneUDot(state, 1), 0.0);
    assertEqual(first.getOneUDot(state, 1), constraint.getAccelerationError(state));
    delete &system;
}

void testNoSlip1DConstraint() {
    
    State state;
    MultibodySystem& system = createSystem();
    SimbodyMatterSubsystem& matter = system.updMatterSubsystem();
    MobilizedBody& first = matter.updMobilizedBody(MobilizedBodyIndex(1));
    MobilizedBody& last = matter.updMobilizedBody(MobilizedBodyIndex(NUM_BODIES));
    Vec3 p(1, 0.5, -2.0);
    UnitVec3 n(0, 1, 0);
    Constraint::NoSlip1D constraint(matter.Ground(), p, n, first, last);
    createState(system, state);
    Vec3 p1 = first.locateGroundPointOnBody(state, p);
    Vec3 p2 = last.locateGroundPointOnBody(state, p);
    Vec3 v1 = first.calcBodyFixedPointVelocityInGround(state, p1);
    Vec3 v2 = last.calcBodyFixedPointVelocityInGround(state, p2);
    Vec3 a1 = first.calcBodyFixedPointAccelerationInGround(state, p1);
    Vec3 a2 = last.calcBodyFixedPointAccelerationInGround(state, p2);
    assertEqual(dot(v1, n), dot(v2, n));
    assertEqual(dot(v1-v2, n), constraint.getVelocityError(state));
    assertEqual(dot(a1-a2, n), 0.0);
    assertEqual(dot(a1-a2, n), constraint.getAccelerationError(state));
    delete &system;
}

void testPointInPlaneConstraint() {
    
    State state;
    MultibodySystem& system = createSystem();
    SimbodyMatterSubsystem& matter = system.updMatterSubsystem();
    MobilizedBody& first = matter.updMobilizedBody(MobilizedBodyIndex(1));
    MobilizedBody& last = matter.updMobilizedBody(MobilizedBodyIndex(NUM_BODIES));
    UnitVec3 normal(1, 0.5, 0);
    Real height = 2.0;
    Vec3 p(1.0, 2.5, -3.0);
    Constraint::PointInPlane constraint(first, normal, height, last, p);
    createState(system, state);
    Vec3 p1 = last.calcBodyPointLocationInBody(state, p, first);
    Vec3 v1 = last.calcStationVelocityInBody(state, p, first);
    assertEqual(dot(p1, normal), height);
    assertEqual(dot(p1, normal)-height, constraint.getPositionError(state));
    assertEqual(dot(v1, normal), 0.0);
    assertEqual(dot(v1, normal), constraint.getVelocityError(state));    
    delete &system;
}

void testPointOnLineConstraint() {
    
    State state;
    MultibodySystem& system = createSystem();
    SimbodyMatterSubsystem& matter = system.updMatterSubsystem();
    MobilizedBody& first = matter.updMobilizedBody(MobilizedBodyIndex(1));
    MobilizedBody& last = matter.updMobilizedBody(MobilizedBodyIndex(NUM_BODIES));
    UnitVec3 dir(1, 0.5, 0);
    Vec3 base(0.5, -0.5, 2.0);
    Vec3 p(1.0, 2.5, -3.0);
    Constraint::PointOnLine constraint(first, dir, base, last, p);
    createState(system, state);
    Vec3 p1 = last.calcBodyPointLocationInBody(state, p, first);
    Vec3 v1 = last.calcStationVelocityInBody(state, p, first);
    assertEqual(cross(p1-base, dir).norm(), 0.0);
    assertEqual(cross(v1, dir).norm(), 0.0);
    delete &system;
}

void testRodConstraint() {
    
    State state;
    MultibodySystem& system = createSystem();
    SimbodyMatterSubsystem& matter = system.updMatterSubsystem();
    MobilizedBody& first = matter.updMobilizedBody(MobilizedBodyIndex(1));
    MobilizedBody& last = matter.updMobilizedBody(MobilizedBodyIndex(NUM_BODIES));
    Constraint::Rod constraint(first, last, 3.0);
    createState(system, state);
    Vec3 r1 = first.getBodyOriginLocation(state);
    Vec3 r2 = last.getBodyOriginLocation(state);
    Vec3 dr = r1-r2;
    Vec3 v1 = first.getBodyOriginVelocity(state);
    Vec3 v2 = last.getBodyOriginVelocity(state);
    Vec3 dv = v1-v2;
    Vec3 a1 = first.getBodyOriginAcceleration(state);
    Vec3 a2 = last.getBodyOriginAcceleration(state);
    Vec3 da = a1-a2;
    assertEqual(dr.norm(), 3.0);
    assertEqual(dr.norm()-3.0, constraint.getPositionError(state));
    assertEqual(dot(dv, dr), 0.0);
    assertEqual(dot(dv, dr), constraint.getVelocityError(state));
    assertEqual(dot(da, dr), -dv.normSqr());
    assertEqual(dot(da, dr)+dv.normSqr(), constraint.getAccelerationError(state));
    delete &system;
}

void testWeldConstraint() {
    
    State state;
    MultibodySystem& system = createSystem();
    SimbodyMatterSubsystem& matter = system.updMatterSubsystem();
    MobilizedBody& first = matter.updMobilizedBody(MobilizedBodyIndex(1));
    MobilizedBody& last = matter.updMobilizedBody(MobilizedBodyIndex(NUM_BODIES));
    Constraint::Weld constraint(first, last);
    createState(system, state);
    assertEqual(first.getBodyOriginLocation(state), last.getBodyOriginLocation(state));
    assertEqual(first.getBodyVelocity(state), last.getBodyVelocity(state));
    assertEqual(first.getBodyAcceleration(state), last.getBodyAcceleration(state));
    
    // Extra constraints are required for assembly.  Without them, this constraint only guarantees
    // that the second body's X/Y/Z axis is perpendicular to the first body's Y/Z/X axis.
    
    Rotation rot1 = first.getBodyRotation(state);
    Rotation rot2 = last.getBodyRotation(state);
    assertEqual(dot(rot1*Vec3(1, 0, 0), rot2*Vec3(0, 1, 0)), 0.0);
    assertEqual(dot(rot1*Vec3(0, 1, 0), rot2*Vec3(0, 0, 1)), 0.0);
    assertEqual(dot(rot1*Vec3(0, 0, 1), rot2*Vec3(1, 0, 0)), 0.0);
    delete &system;
}

int main() {
    try {
        testBallConstraint();
        testConstantAngleConstraint();
        testConstantOrientationConstraint();
        testConstantSpeedConstraint();
        testNoSlip1DConstraint();
        testPointInPlaneConstraint();
        testPointOnLineConstraint();
        testRodConstraint();
        testWeldConstraint();
    }
    catch(const std::exception& e) {
        cout << "exception: " << e.what() << endl;
        return 1;
    }
    cout << "Done" << endl;
    return 0;
}

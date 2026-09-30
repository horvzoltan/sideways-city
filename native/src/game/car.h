// car.h - the car's handling: the arcade drift model. Steering sets the rotation directly, and the
// sideways slide decays at a rate set by the grip, which throttle and the handbrake lower, so the
// rear steps out and stays out. No raylib here, so the tests can drive it headless.
#pragma once
#include <cmath>

// Every tuning value in one place. Speeds are px/s (the speedometer shows px/s x 0.32 km/h).
struct Handling {
    double topSpeed = 500;                      // 160 km/h on the speedometer
    double reverseTop = 220;
    double engineAccel = 540, backwardsAccel = 1100;   // throttle; and throttle while rolling backwards
    double brakeAccel = 950, reverseAccel = 320;       // brake; and reversing once (nearly) stopped
    double rollDrag = 0.5;                      // speed lost per second, as a share of speed
    double handbrakeDrag = 170;
    double grip = 7.5;                          // how fast a sideways slide dies away (per second)
    double powerSpeed = 260, powerLoosen = 4.5; // above powerSpeed, full throttle takes this much grip off: the rear steps out
    double handbrakeGrip = 1.0;                 // grip with the handbrake fully pulled
    double steerRate = 2.8;                     // rotation (rad/s) at full steering
    double steerFullSpeed = 150;                // below this speed steering is weaker
    double handbrakeSteer = 1.4;                // steering is this much stronger with the handbrake pulled
    double steerResponse = 9;                   // how quickly the rotation follows the steering
};

struct CarInput {
    double throttle = 0, brake = 0, steer = 0;   // 0..1, 0..1, -1..1 (right is positive)
    double handbrake = 0;                        // 0..1: a trigger pulls it progressively
    bool digitalSteer = false;                   // keyboard steering (unused by this model, kept for input code)
};

struct CarState {
    double x = 0, y = 0, a = 0, vx = 0, vy = 0, w = 0;   // position, heading, velocity, rotation rate (clockwise on screen)
    double steer = 0;                                    // last steering input, for the debug overlay
};

// Surface under the car: grip multiplier (1 = tarmac) and extra rolling drag (soft sand, gravel).
struct CarSurface { double grip = 1, drag = 0; };

// Advances the car by dt seconds. acc and top scale acceleration and top speed.
void StepCar(CarState& c, const CarInput& in, const CarSurface& s, double acc, double top, double dt, const Handling& h = Handling{});

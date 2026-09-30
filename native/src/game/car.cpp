#include "car.h"
#include <algorithm>

namespace {
double Sign(double v) { return v > 0 ? 1 : v < 0 ? -1 : 0; }
}

void StepCar(CarState& c, const CarInput& in, const CarSurface& s, double acc, double top, double dt, const Handling& h) {
    const double fx = std::cos(c.a), fy = std::sin(c.a), rx = -fy, ry = fx;
    double vf = c.vx * fx + c.vy * fy, vr = c.vx * rx + c.vy * ry;   // forward and sideways speed
    const double thr = in.throttle, brk = in.brake, hb = std::clamp(in.handbrake, 0.0, 1.0);

    // forward speed: engine, brakes, drag, handbrake
    if (thr) vf += (vf < 0 ? h.backwardsAccel : h.engineAccel * acc) * thr * dt * (0.6 + 0.4 * s.grip);
    if (brk) vf -= (vf > 20 ? h.brakeAccel : h.reverseAccel) * brk * dt;
    vf = std::max(-h.reverseTop, std::min(h.topSpeed * top, vf));
    vf -= vf * (h.rollDrag + s.drag) * dt;
    if (hb > 0) vf -= Sign(vf) * std::min(std::fabs(vf), h.handbrakeDrag * hb * dt);
    if (!thr && !brk && std::fabs(vf) < 8) vf = 0;

    // sideways speed dies away with grip; power and the handbrake loosen the rear
    const double power = vf > h.powerSpeed ? h.grip - h.powerLoosen * thr : h.grip;
    const double grip = (power + (h.handbrakeGrip - power) * hb) * s.grip;
    vr *= std::exp(-grip * dt);

    // steering sets the rotation directly (weaker when slow, reversed when going backwards)
    c.steer = in.steer;
    const double target = in.steer * h.steerRate * std::min(1.0, std::fabs(vf) / h.steerFullSpeed) * (vf >= 0 ? 1 : -1) * (1 + (h.handbrakeSteer - 1) * hb);
    c.w += (target - c.w) * std::min(1.0, h.steerResponse * dt);
    c.a += c.w * dt;

    c.vx = fx * vf + rx * vr; c.vy = fy * vf + ry * vr;
    c.x += c.vx * dt; c.y += c.vy * dt;
}

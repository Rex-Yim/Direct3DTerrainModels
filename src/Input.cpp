#include "Input.h"

#include <algorithm>
#include <cmath>

namespace {

float DeadZone(float v, float zone) {
    if (v > zone) {
        return (v - zone) / (1.f - zone);
    }
    if (v < -zone) {
        return (v + zone) / (1.f - zone);
    }
    return 0.f;
}

}  // namespace

void Input::Update() {
    state_ = {};

    if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0) {
        state_.quit = true;
    }

    float throttle = 0.f;
    if ((GetAsyncKeyState('W') & 0x8000) != 0) {
        throttle += 1.f;
    }
    if ((GetAsyncKeyState('S') & 0x8000) != 0) {
        throttle -= 1.f;
    }

    float steer = 0.f;
    if ((GetAsyncKeyState('A') & 0x8000) != 0) {
        steer -= 1.f;
    }
    if ((GetAsyncKeyState('D') & 0x8000) != 0) {
        steer += 1.f;
    }

    float brake = 0.f;
    if ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0) {
        brake = 1.f;
    }

    XINPUT_STATE xis{};
    if (XInputGetState(0, &xis) == ERROR_SUCCESS) {
        const SHORT lx = xis.Gamepad.sThumbLX;
        const float lx_f = DeadZone(static_cast<float>(lx) / 32768.f, 0.2f);
        steer = lx_f;

        const BYTE trig_r = xis.Gamepad.bRightTrigger;
        const BYTE trig_l = xis.Gamepad.bLeftTrigger;
        const float r = static_cast<float>(trig_r) / 255.f;
        const float l = static_cast<float>(trig_l) / 255.f;
        if (r > 0.05f || l > 0.05f) {
            throttle = r - l * 0.6f;
            brake = (std::max)(brake, l * 0.5f);
        }
    }

    state_.throttle = std::clamp(throttle, -1.f, 1.f);
    state_.steer = std::clamp(steer, -1.f, 1.f);
    state_.brake = std::clamp(brake, 0.f, 1.f);
}

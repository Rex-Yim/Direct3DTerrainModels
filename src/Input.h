#pragma once

#include <windows.h>

#include <XInput.h>

/**
 * @brief Keyboard (GetAsyncKeyState) + optional XInput gamepad for vehicle control.
 */
struct InputState {
    float throttle = 0.f;  // -1 reverse .. +1 forward
    float steer = 0.f;     // -1 left .. +1 right
    float brake = 0.f;     // 0..1
    bool quit = false;
    /** True for one Update() after M goes down (rising edge). */
    bool windmill_toggle = false;
};

class Input {
public:
    void Update();

    const InputState& State() const { return state_; }

private:
    InputState state_{};
    bool prev_m_down_ = false;
};

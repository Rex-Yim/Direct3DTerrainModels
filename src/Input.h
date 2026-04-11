#pragma once

#include <windows.h>

#include <XInput.h>

/**
 * @brief Keyboard (GetAsyncKeyState) + optional XInput gamepad for vehicle control.
 */
struct InputState {
    float throttle = 0.f;  // -1 reverse .. +1 forward
    float steer = 0.f;     // -1 left .. +1 right
    float brake = 0.f;   // 0..1
    bool quit = false;
};

class Input {
public:
    void Update();

    const InputState& State() const { return state_; }

private:
    InputState state_{};
};

#pragma once
#include <imgui/imgui.h>
#include <unordered_map>
#include <string>
#include <cmath>
#include <algorithm>

// ============================================================================
//  ui_anim.hpp  —  Animation engine for smooth UI transitions
//  All functions are inline; no external dependencies beyond ImGui + STL.
// ============================================================================

namespace ui_anim {

// ============================================================================
//  §1  EASING FUNCTIONS
//  All take t in [0, 1] and return a value in (approximately) [0, 1].
// ============================================================================

inline float EaseLinear(float t) {
    return t;
}

// ── Quadratic ────────────────────────────────────────────────────────────────

inline float EaseInQuad(float t) {
    return t * t;
}

inline float EaseOutQuad(float t) {
    return 1.f - (1.f - t) * (1.f - t);
}

inline float EaseInOutQuad(float t) {
    return t < 0.5f
        ? 2.f * t * t
        : 1.f - powf(-2.f * t + 2.f, 2.f) * 0.5f;
}

// ── Cubic ────────────────────────────────────────────────────────────────────

inline float EaseInCubic(float t) {
    return t * t * t;
}

inline float EaseOutCubic(float t) {
    return 1.f - powf(1.f - t, 3.f);
}

inline float EaseInOutCubic(float t) {
    return t < 0.5f
        ? 4.f * t * t * t
        : 1.f - powf(-2.f * t + 2.f, 3.f) * 0.5f;
}

// ── Overshoot / Back ─────────────────────────────────────────────────────────

inline float EaseOutBack(float t) {
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.f;
    return 1.f + c3 * powf(t - 1.f, 3.f) + c1 * powf(t - 1.f, 2.f);
}

// ── Elastic ──────────────────────────────────────────────────────────────────

inline float EaseOutElastic(float t) {
    if (t <= 0.f) return 0.f;
    if (t >= 1.f) return 1.f;
    constexpr float c4 = (2.f * 3.14159265f) / 3.f;
    return powf(2.f, -10.f * t) * sinf((t * 10.f - 0.75f) * c4) + 1.f;
}

// ── Bounce ───────────────────────────────────────────────────────────────────

inline float EaseOutBounce(float t) {
    constexpr float n1 = 7.5625f;
    constexpr float d1 = 2.75f;
    if (t < 1.f / d1) {
        return n1 * t * t;
    } else if (t < 2.f / d1) {
        t -= 1.5f / d1;
        return n1 * t * t + 0.75f;
    } else if (t < 2.5f / d1) {
        t -= 2.25f / d1;
        return n1 * t * t + 0.9375f;
    } else {
        t -= 2.625f / d1;
        return n1 * t * t + 0.984375f;
    }
}

// ── Exponential ──────────────────────────────────────────────────────────────

inline float EaseInOutExpo(float t) {
    if (t <= 0.f) return 0.f;
    if (t >= 1.f) return 1.f;
    return t < 0.5f
        ? powf(2.f, 20.f * t - 10.f) * 0.5f
        : (2.f - powf(2.f, -20.f * t + 10.f)) * 0.5f;
}

// ── Dispatch helper ──────────────────────────────────────────────────────────
// Maps easing enum values used by AnimatedFloat/AnimatedColor to a function.
//   0 = linear, 1 = quad, 2 = cubic, 3 = back, 4 = elastic

inline float ApplyEasing(int easing, float t) {
    switch (easing) {
        default:
        case 0: return EaseLinear(t);
        case 1: return EaseOutQuad(t);
        case 2: return EaseOutCubic(t);
        case 3: return EaseOutBack(t);
        case 4: return EaseOutElastic(t);
    }
}

// ============================================================================
//  §2  ANIMATEDFLOAT
// ============================================================================

struct AnimatedFloat {
    float current = 0.f;
    float target  = 0.f;
    float speed   = 8.f;  // interpolation speed (higher = snappier)
    int   easing  = 0;    // 0=linear, 1=quad, 2=cubic, 3=back, 4=elastic

    // Set target; animation will play toward it each Update()
    inline void Set(float val) {
        target = val;
    }

    // Snap both current and target instantly — no animation
    inline void SetImmediate(float val) {
        current = target = val;
    }

    // Read the current animated value
    inline float Get() const {
        return current;
    }

    // True while the value is still moving toward its target
    inline bool IsAnimating() const {
        return fabsf(target - current) > 1e-4f;
    }

    // Advance animation by dt seconds.
    // Uses exponential decay for the "smooth lerp" path (easing==0 / linear)
    // and a normalised-progress variant for the true easing curves.
    inline void Update(float dt) {
        if (!IsAnimating()) {
            current = target;   // snap on arrival
            return;
        }

        if (easing == 0) {
            // Classic exponential decay — framerate-independent lerp
            float factor = 1.f - expf(-speed * dt);
            current = current + (target - current) * factor;
        } else {
            // Use the chosen easing curve.  We model progress as a fraction of
            // a nominal 1-second journey scaled by speed.
            float remaining = fabsf(target - current);
            float step = remaining * (1.f - expf(-speed * dt));
            float raw_t = (remaining > 1e-6f)
                ? std::clamp(step / remaining, 0.f, 1.f)
                : 1.f;
            float eased = ApplyEasing(easing, raw_t);
            current = current + (target - current) * eased;
        }

        // Hard-snap when close enough to avoid endless micro-drift
        if (fabsf(target - current) < 1e-4f)
            current = target;
    }
};

// ============================================================================
//  §3  ANIMATEDCOLOR
// ============================================================================

struct AnimatedColor {
    ImVec4 current = { 0.f, 0.f, 0.f, 0.f };
    ImVec4 target  = { 0.f, 0.f, 0.f, 0.f };
    float  speed   = 6.f;

    inline void Set(ImVec4 col) {
        target = col;
    }

    inline void SetImmediate(ImVec4 col) {
        current = target = col;
    }

    inline ImVec4 Get() const {
        return current;
    }

    inline ImU32 GetU32() const {
        return ImGui::ColorConvertFloat4ToU32(current);
    }

    // Exponential-decay interpolation per channel
    inline void Update(float dt) {
        float factor = 1.f - expf(-speed * dt);
        current.x = current.x + (target.x - current.x) * factor;
        current.y = current.y + (target.y - current.y) * factor;
        current.z = current.z + (target.z - current.z) * factor;
        current.w = current.w + (target.w - current.w) * factor;
    }
};

// ============================================================================
//  §4  GLOBAL ANIMATION MANAGER
// ============================================================================

inline std::unordered_map<std::string, AnimatedFloat> g_floats;
inline std::unordered_map<std::string, AnimatedColor>  g_colors;

// Get-or-create an AnimatedFloat by string ID.
// On first access, initialises it with defaultVal at its current AND target,
// and sets the speed.  Subsequent calls with the same ID return the live ref.
inline AnimatedFloat& Float(const std::string& id,
                             float defaultVal = 0.f,
                             float speed      = 8.f) {
    auto it = g_floats.find(id);
    if (it == g_floats.end()) {
        AnimatedFloat af;
        af.SetImmediate(defaultVal);
        af.speed = speed;
        g_floats.emplace(id, af);
        return g_floats[id];
    }
    return it->second;
}

// Get-or-create an AnimatedColor by string ID.
inline AnimatedColor& Color(const std::string& id,
                             ImVec4 defaultCol = { 0.f, 0.f, 0.f, 0.f },
                             float  speed      = 6.f) {
    auto it = g_colors.find(id);
    if (it == g_colors.end()) {
        AnimatedColor ac;
        ac.SetImmediate(defaultCol);
        ac.speed = speed;
        g_colors.emplace(id, ac);
        return g_colors[id];
    }
    return it->second;
}

// Call once per frame (typically at the top of your render function) to
// advance every managed animation.  Uses ImGui's own DeltaTime so it is
// automatically capped by ImGui's frame-rate logic.
inline void Tick() {
    const float dt = ImGui::GetIO().DeltaTime;
    for (auto& [key, af] : g_floats)
        af.Update(dt);
    for (auto& [key, ac] : g_colors)
        ac.Update(dt);
}

// ============================================================================
//  §5  UTILITY FUNCTIONS
// ============================================================================

// Raw sine pulse: returns [0, 1] oscillating at `speed` Hz.
inline float Pulse(float speed = 1.f) {
    return (sinf((float)ImGui::GetTime() * speed * 6.28318530f) + 1.f) * 0.5f;
}

// Smoothstep-filtered pulse — softer on/off ramps than raw sine.
inline float PulseSmooth(float speed = 1.f) {
    float t = Pulse(speed);
    return t * t * (3.f - 2.f * t); // smoothstep
}

// Modulate the alpha channel of a packed ImU32 colour with a sine pulse.
//  base      — base colour (alpha ignored)
//  intensity — fraction of alpha range to vary (0 = static, 1 = full range)
//  speed     — oscillation frequency in Hz
inline ImU32 PulseColor(ImU32 base, float intensity = 0.3f, float speed = 2.f) {
    float t        = Pulse(speed);
    float baseA    = ((base >> IM_COL32_A_SHIFT) & 0xFF) / 255.f;
    float minAlpha = baseA * (1.f - intensity);
    float alpha    = minAlpha + (baseA - minAlpha) * t;
    alpha          = std::clamp(alpha, 0.f, 1.f);
    return (base & ~(0xFFu << IM_COL32_A_SHIFT))
         | ((ImU32)(alpha * 255.f) << IM_COL32_A_SHIFT);
}

// Spring-physics interpolation.
//  current   — present value (modified in-place via return, NOT by ref)
//  target    — desired value
//  velocity  — spring velocity state (MUST persist across frames; pass by ref)
//  stiffness — spring constant (higher = snappier, more overshoot)
//  damping   — damping coefficient (higher = less oscillation)
//  dt        — time delta in seconds (if 0, uses ImGui DeltaTime)
//
// Returns the new current value; caller should assign it:
//   myVal = SpringInterp(myVal, goal, myVelocity);
inline float SpringInterp(float  current,
                           float  target,
                           float& velocity,
                           float  stiffness = 300.f,
                           float  damping   = 20.f,
                           float  dt        = 0.f) {
    if (dt <= 0.f)
        dt = ImGui::GetIO().DeltaTime;

    // Semi-implicit Euler integration
    float force    = -stiffness * (current - target) - damping * velocity;
    velocity      += force * dt;
    float newVal   = current + velocity * dt;

    // Snap when settled to avoid infinite micro-oscillation
    if (fabsf(target - newVal) < 1e-3f && fabsf(velocity) < 1e-3f) {
        velocity = 0.f;
        return target;
    }
    return newVal;
}

// ============================================================================
//  §6  TAB ANIMATION STATE
// ============================================================================

struct TabAnimState {
    AnimatedFloat indicatorX;      // horizontal position of active tab indicator
    AnimatedFloat indicatorWidth;  // width of active tab indicator
    AnimatedFloat contentAlpha;    // fade in/out on tab switch
    int   currentTab  = 0;
    int   previousTab = -1;
    float switchTime  = 0.f;       // ImGui time at last switch

    TabAnimState() {
        indicatorX.speed     = 12.f;
        indicatorX.easing    = 2;   // EaseOutCubic — smooth slide
        indicatorWidth.speed = 10.f;
        indicatorWidth.easing = 1;  // EaseOutQuad
        contentAlpha.speed   = 10.f;
        contentAlpha.SetImmediate(1.f);
    }

    // Call when the user selects a new tab.
    // `tab`       — the newly selected tab index
    // `tabX`      — pixel X position of the new tab's indicator
    // `tabWidth`  — pixel width of the new tab
    inline void SwitchTo(int tab, float newX = -1.f, float newW = -1.f) {
        if (tab == currentTab) return;

        previousTab = currentTab;
        currentTab  = tab;
        switchTime  = (float)ImGui::GetTime();

        // Slide the indicator to the new tab (keep current target if not supplied)
        float tabX     = (newX >= 0.f) ? newX : indicatorX.target;
        float tabWidth = (newW >= 0.f) ? newW : indicatorWidth.target;
        indicatorX.Set(tabX);
        indicatorWidth.Set(tabWidth);

        // Briefly dip alpha to zero, then fade back in
        contentAlpha.SetImmediate(0.f);
        contentAlpha.Set(1.f);
    }

    // Advance all child animations.  Call once per frame.
    inline void Update() {
        const float dt = ImGui::GetIO().DeltaTime;
        indicatorX.Update(dt);
        indicatorWidth.Update(dt);
        contentAlpha.Update(dt);
    }

    // True while a tab-switch transition is still running.
    inline bool IsTransitioning() const {
        return indicatorX.IsAnimating()
            || indicatorWidth.IsAnimating()
            || contentAlpha.IsAnimating();
    }
};

// Global tab animation state — single instance for the main menu tab bar.
// For multi-level menus, declare additional TabAnimState instances locally.
inline TabAnimState g_tabAnim;

} // namespace ui_anim

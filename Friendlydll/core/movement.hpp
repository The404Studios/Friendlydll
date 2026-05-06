#pragma once
#include "../includes.hpp"

namespace movement {

    inline bool edge_jump = false;
    inline bool edge_jump_duck = false;
    inline bool fast_stop = false;

    inline void EdgeJump(CUserCmd* cmd, C_BasePlayer* lp) {
        if (!edge_jump) return;
        if (!lp) return;

        static bool wasOnGround = false;
        bool onGround = (lp->GetFlags() & FL_ONGROUND) != 0;

        // Edge jump: if we were on ground last tick but aren't now, and we didn't jump,
        // it means we walked off an edge — auto jump to gain max distance
        if (wasOnGround && !onGround && !(cmd->buttons & CUserCmd::IN_JUMP)) {
            cmd->buttons |= CUserCmd::IN_JUMP;
            if (edge_jump_duck)
                cmd->buttons |= CUserCmd::IN_DUCK;
        }

        wasOnGround = onGround;
    }

    inline void FastStop(CUserCmd* cmd, C_BasePlayer* lp) {
        if (!fast_stop) return;
        if (!lp) return;
        if (!(lp->GetFlags() & FL_ONGROUND)) return;

        // If no movement keys are pressed, counterstrafe to stop instantly
        bool pressing = (cmd->forwardmove != 0.f) || (cmd->sidemove != 0.f);
        if (pressing) return;

        Vector vel = lp->GetVelocity();
        float speed = vel.Length2D();
        if (speed < 15.f) return;

        // Calculate the angle to negate velocity
        float velAngle = rad2deg(atan2f(vel.y, vel.x));
        float moveAngle = velAngle - cmd->viewangles.y;
        float moveRad = deg2rad(moveAngle);

        cmd->forwardmove = -cosf(moveRad) * speed;
        cmd->sidemove = sinf(moveRad) * speed;
    }

} // namespace movement

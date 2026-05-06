#pragma once
#include "../includes.hpp"
#include "antiaim.hpp"
#include "fakelag.hpp"
#include "prediction.hpp"
#include "heatmap.hpp"
#include "death_replay.hpp"
#include "killfeed.hpp"
#include "threat_radar.hpp"
#include "player_profiler.hpp"
#include "aim_lines.hpp"
#include "damage_log.hpp"

namespace auto_disguise {

    inline bool enabled = false;
    inline bool g_disguised = false;

    struct DisguiseState {
        bool aimbot, silent, autoshoot, triggerbot, rcs;
        bool antiaim_enabled, fakelag_enabled;
        bool boneskeleton, squareesp, snapline, chams;
        bool entity_esp, minimap, sound_esp;
        bool prediction_enabled, heatmap_enabled, death_replay_enabled;
        bool killfeed_analyzer_enabled, threat_radar_enabled;
        bool player_profiler_enabled, aim_lines_enabled, damage_log_enabled;
    };

    inline DisguiseState g_saved{};
    inline bool g_hasSaved = false;

    inline void Engage() {
        if (g_disguised) return;
        g_disguised = true;

        g_saved.aimbot = config::aimbot;
        g_saved.silent = config::silent;
        g_saved.autoshoot = config::autoshoot;
        g_saved.triggerbot = config::triggerbot;
        g_saved.rcs = config::rcs;
        g_saved.antiaim_enabled = antiaim::enabled;
        g_saved.fakelag_enabled = fakelag::enabled;
        g_saved.boneskeleton = config::boneskeleton;
        g_saved.squareesp = config::squareesp;
        g_saved.snapline = config::snapline;
        g_saved.chams = config::chams;
        g_saved.entity_esp = config::entity_esp;
        g_saved.minimap = config::minimap;
        g_saved.sound_esp = config::sound_esp;
        g_saved.prediction_enabled = prediction::enabled;
        g_saved.heatmap_enabled = heatmap::enabled;
        g_saved.death_replay_enabled = death_replay::enabled;
        g_saved.killfeed_analyzer_enabled = killfeed::analyzer_enabled;
        g_saved.threat_radar_enabled = threat_radar::enabled;
        g_saved.player_profiler_enabled = player_profiler::enabled;
        g_saved.aim_lines_enabled = aim_lines::enabled;
        g_saved.damage_log_enabled = damage_log::enabled;
        g_hasSaved = true;

        config::aimbot = false;
        config::silent = false;
        config::autoshoot = false;
        config::triggerbot = false;
        config::rcs = false;
        antiaim::enabled = false;
        fakelag::enabled = false;
        config::boneskeleton = false;
        config::squareesp = false;
        config::snapline = false;
        config::chams = false;
        config::entity_esp = false;
        config::minimap = false;
        config::sound_esp = false;
        prediction::enabled = false;
        heatmap::enabled = false;
        death_replay::enabled = false;
        killfeed::analyzer_enabled = false;
        threat_radar::enabled = false;
        player_profiler::enabled = false;
        aim_lines::enabled = false;
        damage_log::enabled = false;
    }

    inline void Disengage() {
        if (!g_disguised) return;
        g_disguised = false;

        if (g_hasSaved) {
            config::aimbot = g_saved.aimbot;
            config::silent = g_saved.silent;
            config::autoshoot = g_saved.autoshoot;
            config::triggerbot = g_saved.triggerbot;
            config::rcs = g_saved.rcs;
            antiaim::enabled = g_saved.antiaim_enabled;
            fakelag::enabled = g_saved.fakelag_enabled;
            config::boneskeleton = g_saved.boneskeleton;
            config::squareesp = g_saved.squareesp;
            config::snapline = g_saved.snapline;
            config::chams = g_saved.chams;
            config::entity_esp = g_saved.entity_esp;
            config::minimap = g_saved.minimap;
            config::sound_esp = g_saved.sound_esp;
            prediction::enabled = g_saved.prediction_enabled;
            heatmap::enabled = g_saved.heatmap_enabled;
            death_replay::enabled = g_saved.death_replay_enabled;
            killfeed::analyzer_enabled = g_saved.killfeed_analyzer_enabled;
            threat_radar::enabled = g_saved.threat_radar_enabled;
            player_profiler::enabled = g_saved.player_profiler_enabled;
            aim_lines::enabled = g_saved.aim_lines_enabled;
            damage_log::enabled = g_saved.damage_log_enabled;
        }
    }

    inline void Update() {
        if (!enabled) {
            if (g_disguised) Disengage();
            return;
        }

        bool spectated = config::g_beingSpectated.load(std::memory_order_relaxed);

        if (spectated && !g_disguised)
            Engage();
        else if (!spectated && g_disguised)
            Disengage();
    }

} // namespace auto_disguise

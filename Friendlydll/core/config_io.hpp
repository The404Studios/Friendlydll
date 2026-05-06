#pragma once
#include "../includes.hpp"
#include "prediction.hpp"
#include "death_replay.hpp"
#include "heatmap.hpp"
#include "killfeed.hpp"
#include "movement.hpp"
#include "misc_features.hpp"
#include "hud.hpp"
#include "antiaim.hpp"
#include "freecam.hpp"
#include "fakelag.hpp"
#include "aim_lines.hpp"
#include "damage_log.hpp"
#include "door_memory.hpp"
#include "spawn_detect.hpp"
#include "xray.hpp"
#include "waypoints.hpp"
#include "rage_mode.hpp"
#include "printer_monitor.hpp"
#include "threat_radar.hpp"
#include "player_profiler.hpp"
#include "auto_disguise.hpp"
#include "net_panel.hpp"
#include "tick_exploits.hpp"
#include "voice_exploits.hpp"
#include <fstream>
#include <string>
#include <sstream>
#include <unordered_map>

namespace config_io {

    inline std::string GetConfigPath() {
        char path[MAX_PATH];
        HMODULE hm = NULL;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&GetConfigPath, &hm);
        GetModuleFileNameA(hm, path, MAX_PATH);
        std::string dir(path);
        size_t pos = dir.find_last_of("\\/");
        if (pos != std::string::npos) dir = dir.substr(0, pos + 1);
        return dir + "friendlydll.cfg";
    }

    inline void Save() {
        std::ofstream f(GetConfigPath());
        if (!f.is_open()) return;

        // movement
        f << "bunnyhop=" << config::bunnyhop << "\n";
        f << "autostrafe=" << config::autostrafe << "\n";
        f << "autostrafe_legit=" << config::autostrafe_legit << "\n";
        f << "autostrafe_silent=" << config::autostrafe_silent << "\n";
        f << "autostrafe_directional=" << config::autostrafe_directional << "\n";
        f << "edge_jump=" << movement::edge_jump << "\n";
        f << "edge_jump_duck=" << movement::edge_jump_duck << "\n";
        f << "fast_stop=" << movement::fast_stop << "\n";

        // aiming
        f << "aimbot=" << config::aimbot << "\n";
        f << "silent=" << config::silent << "\n";
        f << "autoshoot=" << config::autoshoot << "\n";
        f << "resolver=" << config::resolver << "\n";
        f << "bone=" << config::bone << "\n";
        f << "aimbot_fov=" << config::aimbot_fov << "\n";
        f << "fov=" << config::fov << "\n";
        f << "aimkey=" << config::aimkey << "\n";
        f << "aim_smooth=" << config::aim_smooth << "\n";
        f << "rcs=" << config::rcs << "\n";

        // esp
        f << "snapline=" << config::snapline << "\n";
        f << "squareesp=" << config::squareesp << "\n";
        f << "chams=" << config::chams << "\n";
        f << "boneskeleton=" << config::boneskeleton << "\n";
        f << "esp_min_dist=" << config::esp_min_dist << "\n";
        f << "esp_max_dist=" << config::esp_max_dist << "\n";
        f << "snapline_color_r=" << config::snapline_color[0] << "\n";
        f << "snapline_color_g=" << config::snapline_color[1] << "\n";
        f << "snapline_color_b=" << config::snapline_color[2] << "\n";
        f << "squareesp_color_r=" << config::squareesp_color[0] << "\n";
        f << "squareesp_color_g=" << config::squareesp_color[1] << "\n";
        f << "squareesp_color_b=" << config::squareesp_color[2] << "\n";
        f << "skeleton_color_r=" << config::skeleton_color[0] << "\n";
        f << "skeleton_color_g=" << config::skeleton_color[1] << "\n";
        f << "skeleton_color_b=" << config::skeleton_color[2] << "\n";
        f << "chams_visible_color_r=" << config::chams_visible_color[0] << "\n";
        f << "chams_visible_color_g=" << config::chams_visible_color[1] << "\n";
        f << "chams_visible_color_b=" << config::chams_visible_color[2] << "\n";
        f << "chams_hidden_color_r=" << config::chams_hidden_color[0] << "\n";
        f << "chams_hidden_color_g=" << config::chams_hidden_color[1] << "\n";
        f << "chams_hidden_color_b=" << config::chams_hidden_color[2] << "\n";
        f << "esp_intel_badges=" << config::esp_intel_badges << "\n";

        // combat
        f << "triggerbot=" << config::triggerbot << "\n";
        f << "triggerbot_key=" << config::triggerbot_key << "\n";
        f << "triggerbot_delay_ms=" << config::triggerbot_delay_ms << "\n";
        f << "triggerbot_head_only=" << config::triggerbot_head_only << "\n";
        f << "backtrack=" << config::backtrack << "\n";
        f << "backtrack_ticks=" << config::backtrack_ticks << "\n";
        f << "backtrack_visualize=" << config::backtrack_visualize << "\n";
        f << "viewpunch_remove=" << config::viewpunch_remove << "\n";
        f << "aim_priority=" << config::aim_priority << "\n";
        f << "aim_w_fov=" << config::aim_w_fov << "\n";
        f << "aim_w_distance=" << config::aim_w_distance << "\n";
        f << "aim_w_health=" << config::aim_w_health << "\n";
        f << "aim_w_threat=" << config::aim_w_threat << "\n";
        f << "aim_target_lock=" << config::aim_target_lock << "\n";
        f << "aim_lock_ticks=" << config::aim_lock_ticks << "\n";

        // entity esp
        f << "entity_esp=" << config::entity_esp << "\n";
        f << "entity_esp_timer_max=" << config::entity_esp_timer_max << "\n";
        f << "entity_esp_printers=" << config::entity_esp_printers << "\n";
        f << "entity_esp_shipments=" << config::entity_esp_shipments << "\n";
        f << "entity_esp_drugs=" << config::entity_esp_drugs << "\n";
        f << "entity_esp_doors=" << config::entity_esp_doors << "\n";
        f << "entity_esp_color_printer_r=" << config::entity_esp_color_printer[0] << "\n";
        f << "entity_esp_color_printer_g=" << config::entity_esp_color_printer[1] << "\n";
        f << "entity_esp_color_printer_b=" << config::entity_esp_color_printer[2] << "\n";
        f << "entity_esp_color_shipment_r=" << config::entity_esp_color_shipment[0] << "\n";
        f << "entity_esp_color_shipment_g=" << config::entity_esp_color_shipment[1] << "\n";
        f << "entity_esp_color_shipment_b=" << config::entity_esp_color_shipment[2] << "\n";
        f << "entity_esp_color_drug_r=" << config::entity_esp_color_drug[0] << "\n";
        f << "entity_esp_color_drug_g=" << config::entity_esp_color_drug[1] << "\n";
        f << "entity_esp_color_drug_b=" << config::entity_esp_color_drug[2] << "\n";
        f << "entity_esp_color_door_r=" << config::entity_esp_color_door[0] << "\n";
        f << "entity_esp_color_door_g=" << config::entity_esp_color_door[1] << "\n";
        f << "entity_esp_color_door_b=" << config::entity_esp_color_door[2] << "\n";

        // hud
        f << "minimap=" << config::minimap << "\n";
        f << "minimap_zoom=" << config::minimap_zoom << "\n";
        f << "fov_circle=" << config::fov_circle << "\n";
        f << "fov_circle_color_r=" << config::fov_circle_color[0] << "\n";
        f << "fov_circle_color_g=" << config::fov_circle_color[1] << "\n";
        f << "fov_circle_color_b=" << config::fov_circle_color[2] << "\n";
        f << "fullbright=" << config::fullbright << "\n";
        f << "sound_esp=" << config::sound_esp << "\n";

        // prediction, death replay, heatmap, killfeed
        f << "prediction_enabled=" << prediction::enabled << "\n";
        f << "prediction_time=" << prediction::predict_time << "\n";
        f << "prediction_alpha=" << prediction::ghost_alpha << "\n";
        f << "death_replay_enabled=" << death_replay::enabled << "\n";
        f << "death_replay_seconds=" << death_replay::replay_seconds << "\n";
        f << "heatmap_enabled=" << heatmap::enabled << "\n";
        f << "heatmap_grid_size=" << heatmap::grid_size << "\n";
        f << "heatmap_opacity=" << heatmap::opacity << "\n";
        f << "heatmap_min_hits=" << heatmap::min_hits << "\n";
        f << "killfeed_enabled=" << killfeed::analyzer_enabled << "\n";
        f << "voice_indicators=" << killfeed::voice_indicators << "\n";
        f << "velocity_graph=" << hud::velocity_graph << "\n";
        f << "spectator_list=" << hud::spectator_list << "\n";

        // anti-aim
        f << "antiaim_enabled=" << antiaim::enabled << "\n";
        f << "antiaim_yaw_mode=" << antiaim::yaw_mode << "\n";
        f << "antiaim_pitch_mode=" << antiaim::pitch_mode << "\n";
        f << "antiaim_jitter_range=" << antiaim::jitter_range << "\n";
        f << "antiaim_spin_speed=" << antiaim::spin_speed << "\n";
        f << "antiaim_desync_offset=" << antiaim::desync_offset << "\n";

        // freecam
        f << "freecam_enabled=" << freecam::enabled << "\n";
        f << "freecam_speed=" << freecam::speed << "\n";

        // fake lag
        f << "fakelag_enabled=" << fakelag::enabled << "\n";
        f << "fakelag_mode=" << fakelag::mode << "\n";
        f << "fakelag_choke_ticks=" << fakelag::choke_ticks << "\n";
        f << "fakelag_max_choke=" << fakelag::max_choke << "\n";
        f << "fakelag_visualize=" << fakelag::visualize << "\n";

        // aim lines
        f << "aim_lines_enabled=" << aim_lines::enabled << "\n";
        f << "aim_lines_length=" << aim_lines::line_length << "\n";
        f << "aim_lines_color_r=" << aim_lines::aim_color[0] << "\n";
        f << "aim_lines_color_g=" << aim_lines::aim_color[1] << "\n";
        f << "aim_lines_color_b=" << aim_lines::aim_color[2] << "\n";

        // damage log
        f << "damage_log_enabled=" << damage_log::enabled << "\n";
        f << "damage_log_indicators=" << damage_log::show_indicators << "\n";
        f << "damage_log_duration=" << damage_log::indicator_duration << "\n";

        // door memory
        f << "door_memory_enabled=" << door_memory::enabled << "\n";
        f << "door_memory_show_esp=" << door_memory::show_on_esp << "\n";

        // spawn detect
        f << "spawn_detect_enabled=" << spawn_detect::enabled << "\n";

        // xray
        f << "xray_enabled=" << xray::enabled << "\n";
        f << "xray_mode=" << xray::mode << "\n";
        f << "xray_wall_alpha=" << xray::wall_alpha << "\n";

        // waypoints
        f << "waypoints_enabled=" << waypoints::enabled << "\n";
        f << "waypoint_count=" << waypoints::g_waypoints.size() << "\n";
        for (int wi = 0; wi < static_cast<int>(waypoints::g_waypoints.size()); ++wi) {
            const auto& wp = waypoints::g_waypoints[wi];
            f << "waypoint_" << wi << "_x=" << wp.pos.x << "\n";
            f << "waypoint_" << wi << "_y=" << wp.pos.y << "\n";
            f << "waypoint_" << wi << "_z=" << wp.pos.z << "\n";
            f << "waypoint_" << wi << "_label=" << wp.label << "\n";
            f << "waypoint_" << wi << "_color_r=" << wp.color[0] << "\n";
            f << "waypoint_" << wi << "_color_g=" << wp.color[1] << "\n";
            f << "waypoint_" << wi << "_color_b=" << wp.color[2] << "\n";
        }

        // printer monitor
        f << "printer_monitor_enabled=" << printer_monitor::enabled << "\n";
        f << "printer_monitor_panel=" << printer_monitor::show_panel << "\n";

        // threat radar
        f << "threat_radar_enabled=" << threat_radar::enabled << "\n";
        f << "threat_radar_show_level=" << threat_radar::show_level << "\n";
        f << "threat_radar_color_esp=" << threat_radar::color_esp << "\n";

        // player profiler
        f << "player_profiler_enabled=" << player_profiler::enabled << "\n";
        f << "player_profiler_paths=" << player_profiler::show_paths << "\n";
        f << "player_profiler_panel=" << player_profiler::show_panel << "\n";

        // auto disguise
        f << "auto_disguise_enabled=" << auto_disguise::enabled << "\n";

        // intel
        f << "admin_alert=" << config::admin_alert << "\n";
        f << "spectator_alert=" << config::spectator_alert << "\n";
        f << "money_tracker=" << config::money_tracker << "\n";

        // stealth
        f << "panic_key=" << config::panic_key << "\n";
        f << "spectator_auto_disable=" << config::spectator_auto_disable << "\n";
        f << "screenshot_cleaner=" << config::screenshot_cleaner << "\n";
        f << "recording_mode=" << config::recording_mode << "\n";
        f << "anti_anticheat=" << config::anti_anticheat << "\n";

        // misc
        f << "chatondeath=" << config::chatondeath << "\n";
        f << "allowcslua=" << config::allowcslua << "\n";
        f << "allowcheats=" << config::allowcheats << "\n";
        f << "longRangeMelee=" << config::longRangeMelee << "\n";
        f << "net_sniffer=" << config::net_sniffer << "\n";
        f << "chat_spy=" << net_panel::chat_spy_enabled << "\n";
        f << "net_file_logging=" << net_panel::g_fileLogging << "\n";

        // tick exploits
        f << "lagswitch=" << tick_exploits::lagswitch_enabled << "\n";
        f << "lagswitch_dur=" << tick_exploits::lagswitch_duration << "\n";
        f << "doubletap=" << tick_exploits::doubletap_enabled << "\n";
        f << "dt_shift=" << tick_exploits::dt_shift_ticks << "\n";
        f << "speedhack=" << tick_exploits::speedhack_enabled << "\n";
        f << "speedhack_factor=" << tick_exploits::speedhack_factor << "\n";

        // world
        f << "crosshair_info=" << config::crosshair_info << "\n";
        f << "killstreak_enabled=" << config::killstreak_enabled << "\n";

        // exploit toggles
        f << "nightVision=" << config::nightVision << "\n";
        f << "hitmarker=" << config::hitmarker << "\n";
        f << "advertRunning=" << config::advertRunning << "\n";
        f << "antiAfk=" << config::antiAfk << "\n";
        f << "lagExploit=" << config::lagExploit << "\n";
        f << "silentWalk=" << config::silentWalk << "\n";
        f << "slideWalk=" << config::slideWalk << "\n";
        f << "lockpickAuto=" << config::lockpickAuto << "\n";
        f << "propAlert=" << config::propAlert << "\n";
        f << "voteBot=" << config::voteBot << "\n";
        f << "propFly=" << config::propFly << "\n";
        f << "killSound=" << config::killSound << "\n";
        f << "entityMagnet=" << config::entityMagnet << "\n";
        f << "ghostMode=" << config::ghostMode << "\n";
        f << "infiniteAmmo=" << config::infiniteAmmo << "\n";
        f << "noRecoilLua=" << config::noRecoilLua << "\n";
        f << "antiCrash=" << config::antiCrash << "\n";
        f << "puppetRecording=" << config::puppetRecording << "\n";
        f << "nameSteal=" << config::nameSteal << "\n";
        f << "antiKick=" << config::antiKick << "\n";
        f << "fakeDeath=" << config::fakeDeath << "\n";
        f << "matWallhack=" << config::matWallhack << "\n";
        f << "lowGrav=" << config::lowGrav << "\n";
        f << "soundSpam=" << config::soundSpam << "\n";
        f << "autoBuy=" << config::autoBuy << "\n";

        // darkrp exploit toggles
        f << "warrantShield=" << config::warrantShield << "\n";
        f << "antiArrestEnabled=" << config::antiArrestEnabled << "\n";
        f << "baseAlarmEnabled=" << config::baseAlarmEnabled << "\n";
        f << "autoBountyEnabled=" << config::autoBountyEnabled << "\n";
        f << "doorAutoClose=" << config::doorAutoClose << "\n";
        f << "proximityAlertEnabled=" << config::proximityAlertEnabled << "\n";
        f << "lootVacuumEnabled=" << config::lootVacuumEnabled << "\n";

        // voice exploits
        f << "voice_exploits_enabled=" << voice_exploits::enabled << "\n";
        f << "voice_intercept_channels=" << voice_exploits::intercept_channels << "\n";
        f << "voice_force_unmute=" << voice_exploits::force_unmute << "\n";
        f << "voice_volume_boost=" << voice_exploits::volume_boost << "\n";
        f << "voice_boost_level=" << voice_exploits::boost_level << "\n";
        f << "voice_activity_esp=" << voice_exploits::activity_esp << "\n";
        f << "voice_direction_arrows=" << voice_exploits::direction_arrows << "\n";
        f << "voice_raid_alert=" << voice_exploits::raid_alert << "\n";
        f << "voice_social_mapper=" << voice_exploits::social_mapper << "\n";
        f << "voice_pattern_profiler=" << voice_exploits::pattern_profiler << "\n";
        f << "voice_radar=" << voice_exploits::voice_radar << "\n";
        f << "voice_file_logger=" << voice_exploits::file_logger << "\n";

        // misc_features
        f << "thirdperson=" << misc_features::thirdperson << "\n";
        f << "thirdperson_dist=" << misc_features::thirdperson_dist << "\n";
        f << "custom_crosshair=" << misc_features::custom_crosshair << "\n";
        f << "crosshair_style=" << misc_features::crosshair_style << "\n";
        f << "crosshair_size=" << misc_features::crosshair_size << "\n";
        f << "crosshair_thickness=" << misc_features::crosshair_thickness << "\n";
        f << "crosshair_color_r=" << misc_features::crosshair_color[0] << "\n";
        f << "crosshair_color_g=" << misc_features::crosshair_color[1] << "\n";
        f << "crosshair_color_b=" << misc_features::crosshair_color[2] << "\n";

        // player list
        f << "whitelistMode=" << config::whitelistMode << "\n";

        f.close();
        spdlog::info("[config] saved to {}", GetConfigPath());
    }

    inline void Load() {
        std::ifstream f(GetConfigPath());
        if (!f.is_open()) return;

        std::unordered_map<std::string, std::string> kv;
        std::string line;
        while (std::getline(f, line)) {
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            kv[line.substr(0, eq)] = line.substr(eq + 1);
        }
        f.close();

        auto getBool = [&](const char* key, bool& out) {
            auto it = kv.find(key);
            if (it != kv.end()) out = (it->second != "0");
        };
        auto getInt = [&](const char* key, int& out) {
            auto it = kv.find(key);
            if (it != kv.end()) out = std::atoi(it->second.c_str());
        };
        auto getFloat = [&](const char* key, float& out) {
            auto it = kv.find(key);
            if (it != kv.end()) out = static_cast<float>(std::atof(it->second.c_str()));
        };

        // movement
        getBool("bunnyhop", config::bunnyhop);
        getBool("autostrafe", config::autostrafe);
        getBool("autostrafe_legit", config::autostrafe_legit);
        getBool("autostrafe_silent", config::autostrafe_silent);
        getBool("autostrafe_directional", config::autostrafe_directional);
        getBool("edge_jump", movement::edge_jump);
        getBool("edge_jump_duck", movement::edge_jump_duck);
        getBool("fast_stop", movement::fast_stop);

        // aiming
        getBool("aimbot", config::aimbot);
        getBool("silent", config::silent);
        getBool("autoshoot", config::autoshoot);
        getBool("resolver", config::resolver);
        getInt("bone", config::bone);
        getFloat("aimbot_fov", config::aimbot_fov);
        getFloat("fov", config::fov);
        getInt("aimkey", config::aimkey);
        getFloat("aim_smooth", config::aim_smooth);
        getBool("rcs", config::rcs);

        // esp
        getBool("snapline", config::snapline);
        getBool("squareesp", config::squareesp);
        getBool("chams", config::chams);
        getBool("boneskeleton", config::boneskeleton);
        getFloat("esp_min_dist", config::esp_min_dist);
        getFloat("esp_max_dist", config::esp_max_dist);
        getFloat("snapline_color_r", config::snapline_color[0]);
        getFloat("snapline_color_g", config::snapline_color[1]);
        getFloat("snapline_color_b", config::snapline_color[2]);
        getFloat("squareesp_color_r", config::squareesp_color[0]);
        getFloat("squareesp_color_g", config::squareesp_color[1]);
        getFloat("squareesp_color_b", config::squareesp_color[2]);
        getFloat("skeleton_color_r", config::skeleton_color[0]);
        getFloat("skeleton_color_g", config::skeleton_color[1]);
        getFloat("skeleton_color_b", config::skeleton_color[2]);
        getFloat("chams_visible_color_r", config::chams_visible_color[0]);
        getFloat("chams_visible_color_g", config::chams_visible_color[1]);
        getFloat("chams_visible_color_b", config::chams_visible_color[2]);
        getFloat("chams_hidden_color_r", config::chams_hidden_color[0]);
        getFloat("chams_hidden_color_g", config::chams_hidden_color[1]);
        getFloat("chams_hidden_color_b", config::chams_hidden_color[2]);
        getBool("esp_intel_badges", config::esp_intel_badges);

        // combat
        getBool("triggerbot", config::triggerbot);
        getInt("triggerbot_key", config::triggerbot_key);
        getInt("triggerbot_delay_ms", config::triggerbot_delay_ms);
        getBool("triggerbot_head_only", config::triggerbot_head_only);
        getBool("backtrack", config::backtrack);
        getInt("backtrack_ticks", config::backtrack_ticks);
        getBool("backtrack_visualize", config::backtrack_visualize);
        getBool("viewpunch_remove", config::viewpunch_remove);
        getBool("aim_priority", config::aim_priority);
        getFloat("aim_w_fov", config::aim_w_fov);
        getFloat("aim_w_distance", config::aim_w_distance);
        getFloat("aim_w_health", config::aim_w_health);
        getFloat("aim_w_threat", config::aim_w_threat);
        getBool("aim_target_lock", config::aim_target_lock);
        getInt("aim_lock_ticks", config::aim_lock_ticks);

        // entity esp
        getBool("entity_esp", config::entity_esp);
        getFloat("entity_esp_timer_max", config::entity_esp_timer_max);
        getBool("entity_esp_printers", config::entity_esp_printers);
        getBool("entity_esp_shipments", config::entity_esp_shipments);
        getBool("entity_esp_drugs", config::entity_esp_drugs);
        getBool("entity_esp_doors", config::entity_esp_doors);
        getFloat("entity_esp_color_printer_r", config::entity_esp_color_printer[0]);
        getFloat("entity_esp_color_printer_g", config::entity_esp_color_printer[1]);
        getFloat("entity_esp_color_printer_b", config::entity_esp_color_printer[2]);
        getFloat("entity_esp_color_shipment_r", config::entity_esp_color_shipment[0]);
        getFloat("entity_esp_color_shipment_g", config::entity_esp_color_shipment[1]);
        getFloat("entity_esp_color_shipment_b", config::entity_esp_color_shipment[2]);
        getFloat("entity_esp_color_drug_r", config::entity_esp_color_drug[0]);
        getFloat("entity_esp_color_drug_g", config::entity_esp_color_drug[1]);
        getFloat("entity_esp_color_drug_b", config::entity_esp_color_drug[2]);
        getFloat("entity_esp_color_door_r", config::entity_esp_color_door[0]);
        getFloat("entity_esp_color_door_g", config::entity_esp_color_door[1]);
        getFloat("entity_esp_color_door_b", config::entity_esp_color_door[2]);

        // hud
        getBool("minimap", config::minimap);
        getFloat("minimap_zoom", config::minimap_zoom);
        getBool("fov_circle", config::fov_circle);
        getFloat("fov_circle_color_r", config::fov_circle_color[0]);
        getFloat("fov_circle_color_g", config::fov_circle_color[1]);
        getFloat("fov_circle_color_b", config::fov_circle_color[2]);
        getBool("fullbright", config::fullbright);
        getBool("sound_esp", config::sound_esp);

        // prediction, death replay, heatmap, killfeed
        getBool("prediction_enabled", prediction::enabled);
        getFloat("prediction_time", prediction::predict_time);
        getFloat("prediction_alpha", prediction::ghost_alpha);
        getBool("death_replay_enabled", death_replay::enabled);
        getFloat("death_replay_seconds", death_replay::replay_seconds);
        getBool("heatmap_enabled", heatmap::enabled);
        getFloat("heatmap_grid_size", heatmap::grid_size);
        getFloat("heatmap_opacity", heatmap::opacity);
        getInt("heatmap_min_hits", heatmap::min_hits);
        getBool("killfeed_enabled", killfeed::analyzer_enabled);
        getBool("voice_indicators", killfeed::voice_indicators);
        getBool("velocity_graph", hud::velocity_graph);
        getBool("spectator_list", hud::spectator_list);

        // anti-aim
        getBool("antiaim_enabled", antiaim::enabled);
        getInt("antiaim_yaw_mode", antiaim::yaw_mode);
        getInt("antiaim_pitch_mode", antiaim::pitch_mode);
        getFloat("antiaim_jitter_range", antiaim::jitter_range);
        getFloat("antiaim_spin_speed", antiaim::spin_speed);
        getFloat("antiaim_desync_offset", antiaim::desync_offset);

        // freecam
        getBool("freecam_enabled", freecam::enabled);
        getFloat("freecam_speed", freecam::speed);

        // fake lag
        getBool("fakelag_enabled", fakelag::enabled);
        getInt("fakelag_mode", fakelag::mode);
        getInt("fakelag_choke_ticks", fakelag::choke_ticks);
        getInt("fakelag_max_choke", fakelag::max_choke);
        getBool("fakelag_visualize", fakelag::visualize);

        // aim lines
        getBool("aim_lines_enabled", aim_lines::enabled);
        getFloat("aim_lines_length", aim_lines::line_length);
        getFloat("aim_lines_color_r", aim_lines::aim_color[0]);
        getFloat("aim_lines_color_g", aim_lines::aim_color[1]);
        getFloat("aim_lines_color_b", aim_lines::aim_color[2]);

        // damage log
        getBool("damage_log_enabled", damage_log::enabled);
        getBool("damage_log_indicators", damage_log::show_indicators);
        getFloat("damage_log_duration", damage_log::indicator_duration);

        // door memory
        getBool("door_memory_enabled", door_memory::enabled);
        getBool("door_memory_show_esp", door_memory::show_on_esp);

        // spawn detect
        getBool("spawn_detect_enabled", spawn_detect::enabled);

        // xray
        getBool("xray_enabled", xray::enabled);
        getInt("xray_mode", xray::mode);
        getFloat("xray_wall_alpha", xray::wall_alpha);

        // waypoints
        getBool("waypoints_enabled", waypoints::enabled);
        {
            int wpCount = 0;
            getInt("waypoint_count", wpCount);
            waypoints::g_waypoints.clear();
            for (int wi = 0; wi < wpCount; ++wi) {
                waypoints::Waypoint wp{};
                std::string prefix = "waypoint_" + std::to_string(wi) + "_";
                getFloat((prefix + "x").c_str(), wp.pos.x);
                getFloat((prefix + "y").c_str(), wp.pos.y);
                getFloat((prefix + "z").c_str(), wp.pos.z);
                auto itLabel = kv.find(prefix + "label");
                if (itLabel != kv.end())
                    strncpy_s(wp.label, itLabel->second.c_str(), 31);
                getFloat((prefix + "color_r").c_str(), wp.color[0]);
                getFloat((prefix + "color_g").c_str(), wp.color[1]);
                getFloat((prefix + "color_b").c_str(), wp.color[2]);
                waypoints::g_waypoints.push_back(wp);
            }
        }

        // printer monitor
        getBool("printer_monitor_enabled", printer_monitor::enabled);
        getBool("printer_monitor_panel", printer_monitor::show_panel);

        // threat radar
        getBool("threat_radar_enabled", threat_radar::enabled);
        getBool("threat_radar_show_level", threat_radar::show_level);
        getBool("threat_radar_color_esp", threat_radar::color_esp);

        // player profiler
        getBool("player_profiler_enabled", player_profiler::enabled);
        getBool("player_profiler_paths", player_profiler::show_paths);
        getBool("player_profiler_panel", player_profiler::show_panel);

        // auto disguise
        getBool("auto_disguise_enabled", auto_disguise::enabled);

        // intel
        getBool("admin_alert", config::admin_alert);
        getBool("spectator_alert", config::spectator_alert);
        getBool("money_tracker", config::money_tracker);

        // stealth
        getInt("panic_key", config::panic_key);
        getBool("spectator_auto_disable", config::spectator_auto_disable);
        getBool("screenshot_cleaner", config::screenshot_cleaner);
        getBool("recording_mode", config::recording_mode);
        getBool("anti_anticheat", config::anti_anticheat);

        // misc
        getBool("chatondeath", config::chatondeath);
        getBool("allowcslua", config::allowcslua);
        getBool("allowcheats", config::allowcheats);
        getBool("longRangeMelee", config::longRangeMelee);
        getBool("net_sniffer", config::net_sniffer);
        getBool("chat_spy", net_panel::chat_spy_enabled);
        getBool("net_file_logging", net_panel::g_fileLogging);

        // tick exploits
        getBool("lagswitch", tick_exploits::lagswitch_enabled);
        getInt("lagswitch_dur", tick_exploits::lagswitch_duration);
        getBool("doubletap", tick_exploits::doubletap_enabled);
        getInt("dt_shift", tick_exploits::dt_shift_ticks);
        getBool("speedhack", tick_exploits::speedhack_enabled);
        getFloat("speedhack_factor", tick_exploits::speedhack_factor);

        // world
        getBool("crosshair_info", config::crosshair_info);
        getBool("killstreak_enabled", config::killstreak_enabled);

        // exploit toggles
        getBool("nightVision", config::nightVision);
        getBool("hitmarker", config::hitmarker);
        getBool("advertRunning", config::advertRunning);
        getBool("antiAfk", config::antiAfk);
        getBool("lagExploit", config::lagExploit);
        getBool("silentWalk", config::silentWalk);
        getBool("slideWalk", config::slideWalk);
        getBool("lockpickAuto", config::lockpickAuto);
        getBool("propAlert", config::propAlert);
        getBool("voteBot", config::voteBot);
        getBool("propFly", config::propFly);
        getBool("killSound", config::killSound);
        getBool("entityMagnet", config::entityMagnet);
        getBool("ghostMode", config::ghostMode);
        getBool("infiniteAmmo", config::infiniteAmmo);
        getBool("noRecoilLua", config::noRecoilLua);
        getBool("antiCrash", config::antiCrash);
        getBool("puppetRecording", config::puppetRecording);
        getBool("nameSteal", config::nameSteal);
        getBool("antiKick", config::antiKick);
        getBool("fakeDeath", config::fakeDeath);
        getBool("matWallhack", config::matWallhack);
        getBool("lowGrav", config::lowGrav);
        getBool("soundSpam", config::soundSpam);
        getBool("autoBuy", config::autoBuy);

        // darkrp exploit toggles
        getBool("warrantShield", config::warrantShield);
        getBool("antiArrestEnabled", config::antiArrestEnabled);
        getBool("baseAlarmEnabled", config::baseAlarmEnabled);
        getBool("autoBountyEnabled", config::autoBountyEnabled);
        getBool("doorAutoClose", config::doorAutoClose);
        getBool("proximityAlertEnabled", config::proximityAlertEnabled);
        getBool("lootVacuumEnabled", config::lootVacuumEnabled);

        // voice exploits
        getBool("voice_exploits_enabled", voice_exploits::enabled);
        getBool("voice_intercept_channels", voice_exploits::intercept_channels);
        getBool("voice_force_unmute", voice_exploits::force_unmute);
        getBool("voice_volume_boost", voice_exploits::volume_boost);
        getFloat("voice_boost_level", voice_exploits::boost_level);
        getBool("voice_activity_esp", voice_exploits::activity_esp);
        getBool("voice_direction_arrows", voice_exploits::direction_arrows);
        getBool("voice_raid_alert", voice_exploits::raid_alert);
        getBool("voice_social_mapper", voice_exploits::social_mapper);
        getBool("voice_pattern_profiler", voice_exploits::pattern_profiler);
        getBool("voice_radar", voice_exploits::voice_radar);
        getBool("voice_file_logger", voice_exploits::file_logger);

        // misc_features
        getBool("thirdperson", misc_features::thirdperson);
        getFloat("thirdperson_dist", misc_features::thirdperson_dist);
        getBool("custom_crosshair", misc_features::custom_crosshair);
        getInt("crosshair_style", misc_features::crosshair_style);
        getFloat("crosshair_size", misc_features::crosshair_size);
        getFloat("crosshair_thickness", misc_features::crosshair_thickness);
        getFloat("crosshair_color_r", misc_features::crosshair_color[0]);
        getFloat("crosshair_color_g", misc_features::crosshair_color[1]);
        getFloat("crosshair_color_b", misc_features::crosshair_color[2]);

        // player list
        getBool("whitelistMode", config::whitelistMode);

        spdlog::info("[config] loaded from {}", GetConfigPath());
    }

} // namespace config_io

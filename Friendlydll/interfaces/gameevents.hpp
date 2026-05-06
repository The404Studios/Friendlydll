#pragma once
#include "../includes.hpp"
class DamageEvent : public IGameEventListener2
{
public:
	DamageEvent(void) {};
	~DamageEvent(void) {};
	void FireGameEvent(IGameEvent* event) override
	{
		if (!event)
			return;
		int localPlayerID = interfaces::engine->GetLocalPlayer();
		int target = interfaces::engine->GetPlayerForUserID(event->GetInt("userid")); // UserID of the victim
		int attacker = interfaces::engine->GetPlayerForUserID(event->GetInt("attacker")); // UserID of the attacker

		if (!target || !attacker) return;

		player_info_s targetInfo{};
		player_info_s attackerInfo{};
		if (!interfaces::engine->GetPlayerInfo(target, &targetInfo)) return;
		if (!interfaces::engine->GetPlayerInfo(attacker, &attackerInfo)) return;

		if (strlen(attackerInfo.name) && attacker != localPlayerID)
			//std::cout << attackerInfo.name << " attacked " << targetInfo.name << ". NEW HP: " << event->GetInt("health") << std::endl;
			spdlog::default_logger()->info(std::format("{} attacked {}", attackerInfo.name, targetInfo.name));

		if (target == localPlayerID || attacker != localPlayerID)
			return;

		//if (Settings::Misc::hitmarkerSoundEnabled)
		//	MatSystemSurface->PlaySound(hitMarkers[Settings::Misc::hitmarkerSound]);

		//Settings::lastHitmarkerTime = EngineClient->Time();
	}
};
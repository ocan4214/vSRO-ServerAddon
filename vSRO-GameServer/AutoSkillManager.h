#pragma once
#include <Windows.h>
#include <cstdint>
#include "CustomOpcodes.h"

class CGObjPC;

// ============================================================================
// AutoSkillManager - automatic mastery / skill level-up (custom opcode 0x7712)
// ============================================================================
// The client (DevKit Skill window, "Max Mastery" / "Max Skill" boxes per mastery) sends:
//   action 1 / 0 (enable / disable): u32 masteryId, u8 flags   -> logged only
//   action 2 (tick, every ~2 s while a box is ticked): u8 count, count x {u32 masteryId, u8 flags}
//   flags: bit0 = raise the mastery, bit1 = raise the skills of that mastery.
// The server does all checks and the learning itself, on the game thread (inside
// CGObjPC::OnPacketReceive):
// - only when the player is SAFE: alive and no HP loss for SAFE_SECONDS (a player in town takes no
//   damage, so "in town" is covered by the same rule);
// - mastery: same checks as the native LearnMastery 0x0059C4E0 (char level, total cap CH/EU, SP) are done
//   first, so the native call never fails and never sends an error box to the client;
// - skills: only skill ids whose first required mastery is that mastery, and only when the native
//   validator 0x0059E450 answers 0x3400 (OK) - then the native LearnSkill 0x0059BFE0 does the rest.
// The client only says WHAT to raise; it can not raise anything the server would not allow.
// ============================================================================
class AutoSkillManager
{
public:
	static void Init();
	static bool IsEnabled();

	// Opcode 0x7712. Returns true if the packet was handled.
	static bool HandlePacket(void* pPlayer, void* pPacket);

	// Called for every packet a player sends: samples HP for the "not taking damage" rule.
	static void OnAnyPacket(void* pPlayer);

private:
	static bool m_bEnabled;
	static uint32_t m_SafeSeconds;
	static uint32_t m_MaxLearnPerTick;
	static bool m_bChatNotice;
};

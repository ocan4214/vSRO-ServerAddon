#pragma once
#include <Windows.h>
#include <cstdint>
#include "CustomOpcodes.h"

class CGObjPC;

// ============================================================================
// BulkStatManager - Modular vSRO Bulk Stat Allocation Subsystem
// ============================================================================
// Responsibilities:
// - Validates custom opcode 0x7710 packet payload and boundaries.
// - Handles smallint overflow protection (clamped at 32000).
// - Calls native Joymax stat allocator (0x00511470).
// - Synchronizes client via 0x303D (SendStatUpdate) and 0x304E (SendStuffUpdate).
// - Provides in-game feedback via SendChatMessage and logs to vSRO-MobKillStats.log.
// ============================================================================
class BulkStatManager
{
public:
	// Initialize configuration from vSRO-GameServer.ini
	static void Init();

	// Check if bulk stat feature is enabled
	static bool IsEnabled();

	// Central packet handler for CGObjPC::OnPacketReceive (0x0050EEE0)
	// Returns true if packet was handled (skips Joymax unhandled packet DC)
	static bool HandlePacket(void* pPlayer, void* pPacket);

	// Core stat allocation logic
	static void HandleStatAllocation(CGObjPC* pPlayer, uint8_t statType);

private:
	static bool m_bEnabled;
};

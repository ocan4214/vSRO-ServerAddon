#include "BulkStatManager.h"
#include "AppManager.h"
#include "Silkroad/Object/CGObjPC.h"
#include "Utils/IO/SimpleIni.h"
#include <cstdio>

bool BulkStatManager::m_bEnabled = true;

void BulkStatManager::Init()
{
	CSimpleIniA ini;
	ini.LoadFile("vSRO-GameServer.ini");

	m_bEnabled = ini.GetBoolValue("BulkStat", "ENABLED", true);
	if (m_bEnabled)
	{
		AppManager::LogMobKill("[BulkStat] ENABLED | Modular Custom Opcode 0x7710 Wire Protocol Active");
	}
	else
	{
		AppManager::LogMobKill("[BulkStat] DISABLED");
	}
}

bool BulkStatManager::IsEnabled()
{
	return m_bEnabled;
}

bool BulkStatManager::HandlePacket(void* pPlayer, void* pPacket)
{
	if (!pPlayer || !pPacket)
		return false;

	if (!m_bEnabled)
		return false;

	__try
	{
		char* pBuffer = *reinterpret_cast<char**>(reinterpret_cast<char*>(pPacket) + 0x1034);
		uint16_t cursor = *reinterpret_cast<uint16_t*>(reinterpret_cast<char*>(pPacket) + 0x103c);
		uint16_t end = *reinterpret_cast<uint16_t*>(reinterpret_cast<char*>(pPacket) + 0x103e);
		uint16_t* pDataSize = *reinterpret_cast<uint16_t**>(reinterpret_cast<char*>(pPacket) + 0x1054);
		uint16_t payloadLen = pDataSize ? (*pDataSize & 0x7FFF) : (end > cursor ? (end - cursor) : 0);

		if (!pBuffer || (payloadLen < 1 && end <= cursor))
		{
			AppManager::LogMobKill("[BulkStat FAIL] 0x7710 rejected: empty payload (payloadLen=%u, cursor=%u, end=%u)",
				payloadLen, cursor, end);
			return false;
		}

		uint8_t statType = 0xFF;
		if (cursor < end)
		{
			statType = static_cast<uint8_t>(pBuffer[cursor]);
			*reinterpret_cast<uint16_t*>(reinterpret_cast<char*>(pPacket) + 0x103c) = cursor + 1;
		}
		else if (pBuffer && payloadLen >= 1)
		{
			statType = static_cast<uint8_t>(pBuffer[0]);
		}

		// Boundary check: statType must be 0 (STR) or 1 (INT)
		if (statType > 1)
		{
			static_cast<CGObjPC*>(pPlayer)->SendChatMessage(6, "[Server]", "Hata: Gecersiz stat tipi secildi!");
			AppManager::LogMobKill("[BulkStat FAIL] 0x7710 rejected: invalid statType=%u (cursor=%u, end=%u, payloadLen=%u)",
				statType, cursor, end, payloadLen);
			return false;
		}

		AppManager::LogMobKill("[BulkStat] 0x7710 received from player | statType=%u (%s) | cursor=%u, payloadLen=%u",
			statType, statType == BULK_STAT_TYPE_INT ? "INT" : "STR", cursor, payloadLen);

		HandleStatAllocation(static_cast<CGObjPC*>(pPlayer), statType);
		return true; // handled, suppress Joymax unhandled packet disconnect (0x004B0DD0)
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		AppManager::LogMobKill("[BulkStat ERROR] Exception caught in BulkStatManager::HandlePacket");
	}

	return false;
}

void BulkStatManager::HandleStatAllocation(CGObjPC* pPlayer, uint8_t statType)
{
	if (!pPlayer) return;

	bool isInt = (statType == BULK_STAT_TYPE_INT);

	CInstancePC* pInstance = pPlayer->GetInstance();
	if (!pInstance) return;

	// 1. Boundary check: Free stat points
	uint16_t freePoints = pInstance->RemainStatPoint;
	if (freePoints == 0)
	{
		pPlayer->SendChatMessage(6, "[Server]", "Dagitilacak serbest stat puaniniz bulunmamaktadir.");
		AppManager::LogMobKill("[BulkStat FAIL] Char: '%s' (ID: %u) Reason: No free stat points available",
			pInstance->CharName16, pInstance->CharID);
		return;
	}

	// 2. Boundary check: 32000 smallint overflow ceiling
	uint16_t currentStat = isInt ? pInstance->Intelect : pInstance->Strength;
	if (currentStat >= 32000)
	{
		char msg[128];
		sprintf_s(msg, "Hata: %s limiti (32000) zaten dolu!", isInt ? "INT" : "STR");
		pPlayer->SendChatMessage(6, "[Server]", msg);
		AppManager::LogMobKill("[BulkStat FAIL] Char: '%s' (ID: %u) Reason: %s limit (32000) already reached (current=%u)",
			pInstance->CharName16, pInstance->CharID, isInt ? "INT" : "STR", currentStat);
		return;
	}

	// Clamp to 32000 limit so excess free points are safely retained
	uint16_t pointsToAlloc = freePoints;
	if ((uint32_t)currentStat + pointsToAlloc > 32000)
	{
		pointsToAlloc = static_cast<uint16_t>(32000 - currentStat);
	}

	uint32_t strPoints = isInt ? 0 : pointsToAlloc;
	uint32_t intPoints = isInt ? pointsToAlloc : 0;

	// 3. Call Joymax native stat allocator at 0x00511470
	// Handles: deduction from RemainStatPoint, dirty flag 0x10,
	//          addition to Strength/Intelect, dirty flag 0x04,
	//          stat recalculation (HP, MP, Attack, Defense)
	uint32_t retVal = 0;
	void* pObj = pPlayer;
	__asm
	{
		push ebx
		push esi
		push edi

		push intPoints
		mov eax, strPoints
		mov ebx, pObj
		mov edx, 0x00511470
		call edx

		mov retVal, eax
		pop edi
		pop esi
		pop ebx
	}

	if (retVal == 1)
	{
		// 4. Dispatch Joymax live sync packets to client
		pPlayer->SendStatUpdate();         // 0x303D: syncs STR, INT, Max HP, Max MP, Attack, Defense
		pPlayer->SendStuffUpdate(3, false); // 0x304E: type 3 = syncs RemainStatPoint

		// 5. In-game feedback notice to player
		char notice[128];
		if (pointsToAlloc < freePoints)
		{
			sprintf_s(notice, "Tebrikler! %s 32000 limitine ulasti (+%u verildi, kalan: %u).",
				isInt ? "INT" : "STR", pointsToAlloc, pInstance->RemainStatPoint);
		}
		else
		{
			sprintf_s(notice, "Tebrikler! %u stat puaninizin tumu %s'ye verildi!",
				pointsToAlloc, isInt ? "INT" : "STR");
		}
		pPlayer->SendChatMessage(6, "[Server]", notice);

		// 6. Server audit log (console & file)
		AppManager::LogMobKill("[BulkStat SUCCESS] Char: '%s' (ID: %u) |Opcode 0x7710| allocated %u points to %s -> Total %s: %u | Remaining: %u",
			pInstance->CharName16, pInstance->CharID, pointsToAlloc,
			isInt ? "INT" : "STR", isInt ? "INT" : "STR",
			isInt ? pInstance->Intelect : pInstance->Strength,
			pInstance->RemainStatPoint);
	}
	else
	{
		pPlayer->SendChatMessage(6, "[Server]", "Hata: Stat puanlari dagitilamadi!");
		AppManager::LogMobKill("[BulkStat FAIL] Char: '%s' (ID: %u) Reason: Native allocator 0x00511470 returned failure (attempted %u to %s)",
			pInstance->CharName16, pInstance->CharID, pointsToAlloc, isInt ? "INT" : "STR");
	}
}

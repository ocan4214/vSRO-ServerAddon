#include "AutoSkillManager.h"
#include "AppManager.h"
#include "Silkroad/Object/CGObjPC.h"
#include "Utils/IO/SimpleIni.h"
#include <cstdio>
#include <map>
#include <mutex>
#include <vector>
#include <algorithm>

bool AutoSkillManager::m_bEnabled = true;
uint32_t AutoSkillManager::m_SafeSeconds = 5;
uint32_t AutoSkillManager::m_MaxLearnPerTick = 10;
bool AutoSkillManager::m_bChatNotice = true;

namespace
{
	// ---- SR_GameServer addresses (vSRO 1.188, verified by disassembly 2026-11) ----
	const uintptr_t ADDR_MASTERY_ENTRY   = 0x0059D6F0; // eax = skill component, push masteryId -> entry or 0
	const uintptr_t ADDR_MASTERY_TOTAL   = 0x0059E7C0; // eax = skill component -> sum of all mastery levels
	const uintptr_t ADDR_LEARN_MASTERY   = 0x0059C4E0; // thiscall(component, u32 masteryId, u8 count) -> 1 ok
	const uintptr_t ADDR_SKILL_VALIDATE  = 0x0059E450; // stdcall(component, skillId) -> ax error code
	const uintptr_t ADDR_LEARN_SKILL     = 0x0059BFE0; // thiscall(component, u32 skillId)
	const uintptr_t ADDR_SKILL_RECORD    = 0x0046C1F0; // eax = [DATA_MGR]+0x888, push skillId -> record or 0
	const uintptr_t ADDR_MASTERY_SP_ROW  = 0x00411130; // ecx = [DATA_MGR], eax = level -> row, SP cost at +0x10
	const uintptr_t ADDR_DATA_MGR        = 0x00D6AA14; // global pointer to the game data manager
	const uintptr_t ADDR_CH_MASTERY_CAP  = 0x0059C5E7; // imm32 of "cmp eax, 0x14a" (patched from the ini)
	const uintptr_t ADDR_EU_MASTERY_CAP  = 0x00B46130; // double
	const uint32_t  OFS_SKILL_COMPONENT  = 0xA30;      // CGObjPC + 0xA30
	const uint32_t  VT_GET_LEVEL         = 0x124;      // CGObjPC vtable: uint8 GetLevel()
	const uint32_t  VT_AFTER_MASTERY     = 0x50C;      // CGObjPC vtable: called with 0x20 after a mastery-up
	const uint16_t  SKILL_OK             = 0x3400;

	const uint8_t FLAG_MASTERY = 1;
	const uint8_t FLAG_SKILL   = 2;

	struct PlayerState
	{
		uint32_t lastHealth;
		DWORD    lastDamageTick;
		DWORD    lastTick;
		bool     wasSafe;
	};

	std::mutex s_Mutex;
	std::map<uint32_t, PlayerState> s_Players;                // CharID -> state (only players who used it)
	std::map<uint32_t, std::vector<uint32_t> > s_SkillsByMastery; // masteryId -> skill ids (first req mastery)
	bool s_SkillTableBuilt = false;

	void* GetComponent(CGObjPC* pc)
	{
		return reinterpret_cast<char*>(pc) + OFS_SKILL_COMPONENT;
	}

	void* GetMasteryEntry(void* comp, uint32_t masteryId)
	{
		void* result = 0;
		__asm
		{
			push masteryId
			mov eax, comp
			mov edx, ADDR_MASTERY_ENTRY
			call edx
			mov result, eax
		}
		return result;
	}

	int GetMasteryLevel(void* entry)
	{
		char* data = *reinterpret_cast<char**>(reinterpret_cast<char*>(entry) + 8);
		return data ? *reinterpret_cast<uint8_t*>(data + 0x20) : -1;
	}

	uint32_t GetMasteryTotal(void* comp)
	{
		uint32_t result = 0;
		__asm
		{
			mov eax, comp
			mov edx, ADDR_MASTERY_TOTAL
			call edx
			mov result, eax
		}
		return result;
	}

	uint8_t GetCharLevel(CGObjPC* pc)
	{
		uintptr_t* vt = *reinterpret_cast<uintptr_t**>(pc);
		return reinterpret_cast<uint8_t(__thiscall*)(CGObjPC*)>(vt[VT_GET_LEVEL / 4])(pc);
	}

	// SP needed to go from 'level' to 'level + 1' (the native loop reads the row of the current level).
	// Returns false when the table has no row.
	bool GetMasterySPCost(uint32_t level, uint32_t* outCost)
	{
		if (level == 0 || level > 0x78)
		{
			*outCost = 0; // level 0 -> 1 is free; above 120 the native code adds nothing
			return true;
		}
		uint32_t mgr = *reinterpret_cast<uint32_t*>(ADDR_DATA_MGR);
		if (!mgr)
			return false;
		void* row = 0;
		__asm
		{
			mov ecx, mgr
			mov eax, level
			mov edx, ADDR_MASTERY_SP_ROW
			call edx
			mov row, eax
		}
		if (!row)
			return false;
		*outCost = *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(row) + 0x10);
		return true;
	}

	bool LearnMastery(CGObjPC* pc, uint32_t masteryId)
	{
		void* comp = GetComponent(pc);
		int ok = reinterpret_cast<int(__thiscall*)(void*, uint32_t, uint8_t)>(ADDR_LEARN_MASTERY)(comp, masteryId, 1);
		if (ok != 1)
			return false;
		// Same follow-up the native 0x70A2 handler (0x00515AB0) does after a successful mastery-up.
		uintptr_t* vt = *reinterpret_cast<uintptr_t**>(pc);
		reinterpret_cast<void(__thiscall*)(CGObjPC*, uint32_t)>(vt[VT_AFTER_MASTERY / 4])(pc, 0x20);
		return true;
	}

	uint16_t ValidateSkill(void* comp, uint32_t skillId)
	{
		uint32_t r = reinterpret_cast<uint32_t(__stdcall*)(void*, uint32_t)>(ADDR_SKILL_VALIDATE)(comp, skillId);
		return static_cast<uint16_t>(r & 0xFFFF);
	}

	void LearnSkill(void* comp, uint32_t skillId)
	{
		reinterpret_cast<void(__thiscall*)(void*, uint32_t)>(ADDR_LEARN_SKILL)(comp, skillId);
	}

	void* GetSkillRecord(uint32_t skillId)
	{
		uint32_t mgr = *reinterpret_cast<uint32_t*>(ADDR_DATA_MGR);
		if (!mgr)
			return 0;
		uint32_t table = mgr + 0x888;
		void* result = 0;
		__asm
		{
			push skillId
			mov eax, table
			mov edx, ADDR_SKILL_RECORD
			call edx
			mov result, eax
		}
		return result;
	}

	// Once: every skill id whose first required mastery (+0xA0) is set, grouped by that mastery.
	void BuildSkillTable()
	{
		if (s_SkillTableBuilt)
			return;
		s_SkillTableBuilt = true;
		uint32_t found = 0;
		for (uint32_t id = 1; id < 0x10000; ++id)
		{
			char* rec = reinterpret_cast<char*>(GetSkillRecord(id));
			if (!rec)
				continue;
			uint32_t reqMastery = *reinterpret_cast<uint32_t*>(rec + 0xA0);
			if (reqMastery == 0)
				continue;
			s_SkillsByMastery[reqMastery].push_back(id);
			++found;
		}
		AppManager::LogMobKill("[AutoSkill] skill table built: %u learnable skills in %u masteries",
			found, (uint32_t)s_SkillsByMastery.size());
	}

	uint32_t GetSkillGroup(uint32_t skillId)
	{
		char* rec = reinterpret_cast<char*>(GetSkillRecord(skillId));
		return rec ? *reinterpret_cast<uint32_t*>(rec + 0x8) : skillId;
	}

	// Raises one mastery level when every native check would pass. Returns true on success.
	bool TryMasteryUp(CGObjPC* pc, CInstancePC* inst, uint32_t masteryId, int* outNewLevel)
	{
		void* comp = GetComponent(pc);
		void* entry = GetMasteryEntry(comp, masteryId);
		if (!entry)
			return false;
		int level = GetMasteryLevel(entry);
		if (level < 0)
			return false;

		uint8_t charLevel = GetCharLevel(pc);
		if (level + 1 > charLevel)
			return false;

		uint32_t total = GetMasteryTotal(comp) + 1;
		uint8_t race = *reinterpret_cast<uint8_t*>(inst->RefObjCharPtr + 0x88);
		if (race == 0)
		{
			int32_t cap = *reinterpret_cast<int32_t*>(ADDR_CH_MASTERY_CAP);
			if ((int32_t)total > cap)
				return false;
		}
		else if (race == 1)
		{
			double cap = *reinterpret_cast<double*>(ADDR_EU_MASTERY_CAP);
			if ((double)total > cap)
				return false;
			if ((double)charLevel * 2.0 < (double)total)
				return false;
		}
		else if (race != 2)
		{
			return false;
		}

		uint32_t cost = 0;
		if (!GetMasterySPCost((uint32_t)level, &cost))
			return false;
		if (cost > inst->RemainSkillPoint)
			return false;

		if (!LearnMastery(pc, masteryId))
			return false;
		*outNewLevel = GetMasteryLevel(entry);
		return true;
	}

	// One round over the skills of a mastery: at most one level per skill group, so SP is spread
	// over all skills instead of one skill first. Returns how many levels were learned.
	uint32_t SkillPass(CGObjPC* pc, uint32_t masteryId, uint32_t budget)
	{
		std::map<uint32_t, std::vector<uint32_t> >::const_iterator it = s_SkillsByMastery.find(masteryId);
		if (it == s_SkillsByMastery.end())
			return 0;
		void* comp = GetComponent(pc);
		std::vector<uint32_t> groupsDone;
		uint32_t learned = 0;
		for (size_t i = 0; i < it->second.size() && learned < budget; ++i)
		{
			uint32_t skillId = it->second[i];
			if (ValidateSkill(comp, skillId) != SKILL_OK)
				continue;
			uint32_t group = GetSkillGroup(skillId);
			if (std::find(groupsDone.begin(), groupsDone.end(), group) != groupsDone.end())
				continue;
			LearnSkill(comp, skillId);
			// The native call has its own checks; count it only if the skill really is learned now.
			if (ValidateSkill(comp, skillId) == SKILL_OK)
				continue;
			groupsDone.push_back(group);
			++learned;
		}
		return learned;
	}

	bool ReadBytes(void* pPacket, void* out, uint16_t size)
	{
		char* base = reinterpret_cast<char*>(pPacket);
		char* buffer = *reinterpret_cast<char**>(base + 0x1034);
		uint16_t* pCursor = reinterpret_cast<uint16_t*>(base + 0x103c);
		uint16_t end = *reinterpret_cast<uint16_t*>(base + 0x103e);
		if (!buffer || *pCursor + size > end)
			return false;
		memcpy(out, buffer + *pCursor, size);
		*pCursor = static_cast<uint16_t>(*pCursor + size);
		return true;
	}

	bool IsSafe(PlayerState& st, CInstancePC* inst, uint32_t safeSeconds)
	{
		if (inst->Health == 0)
			return false;
		return (GetTickCount() - st.lastDamageTick) >= safeSeconds * 1000;
	}

	void SampleHealth(PlayerState& st, CInstancePC* inst)
	{
		if (inst->Health < st.lastHealth)
			st.lastDamageTick = GetTickCount();
		st.lastHealth = inst->Health;
	}
}

void AutoSkillManager::Init()
{
	CSimpleIniA ini;
	ini.LoadFile("vSRO-GameServer.ini");

	m_bEnabled = ini.GetBoolValue("AutoSkillUp", "ENABLED", true);
	m_SafeSeconds = (uint32_t)ini.GetLongValue("AutoSkillUp", "SAFE_SECONDS", 5);
	m_MaxLearnPerTick = (uint32_t)ini.GetLongValue("AutoSkillUp", "MAX_LEARN_PER_TICK", 10);
	m_bChatNotice = ini.GetBoolValue("AutoSkillUp", "CHAT_NOTICE", true);
	if (m_MaxLearnPerTick == 0)
		m_MaxLearnPerTick = 1;

	if (m_bEnabled)
		AppManager::LogMobKill("[AutoSkill] ENABLED | opcode 0x7712 | safe after %u s without damage | max %u levels per tick",
			m_SafeSeconds, m_MaxLearnPerTick);
	else
		AppManager::LogMobKill("[AutoSkill] DISABLED");
}

bool AutoSkillManager::IsEnabled()
{
	return m_bEnabled;
}

static void HandlePacketInner(CGObjPC* pc, void* pPacket, uint32_t safeSeconds, uint32_t maxLearnPerTick, bool chatNotice);

// SEH (__try) can not live in a function with C++ objects to unwind, so the work is in the *Inner
// functions and the public ones only guard them.
static void OnAnyPacketInner(CGObjPC* pc)
{
	CInstancePC* inst = pc->GetInstance();
	if (!inst)
		return;
	std::lock_guard<std::mutex> lock(s_Mutex);
	std::map<uint32_t, PlayerState>::iterator it = s_Players.find(inst->CharID);
	if (it != s_Players.end())
		SampleHealth(it->second, inst);
}

void AutoSkillManager::OnAnyPacket(void* pPlayer)
{
	if (!m_bEnabled || !pPlayer)
		return;
	__try
	{
		OnAnyPacketInner(static_cast<CGObjPC*>(pPlayer));
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

bool AutoSkillManager::HandlePacket(void* pPlayerRaw, void* pPacket)
{
	if (!m_bEnabled || !pPlayerRaw || !pPacket)
		return false;
	__try
	{
		HandlePacketInner(static_cast<CGObjPC*>(pPlayerRaw), pPacket, m_SafeSeconds, m_MaxLearnPerTick, m_bChatNotice);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		AppManager::LogMobKill("[AutoSkill ERROR] Exception caught in AutoSkillManager::HandlePacket");
	}
	return true; // always ours: never let the native handler see 0x7712 (unknown opcode -> disconnect)
}

static void HandlePacketInner(CGObjPC* pc, void* pPacket, uint32_t safeSeconds, uint32_t maxLearnPerTick, bool chatNotice)
{
	{
		CInstancePC* inst = pc->GetInstance();
		if (!inst)
			return;

		uint8_t action = 0xFF;
		if (!ReadBytes(pPacket, &action, 1))
			return;

		if (action == AUTO_SKILL_ACTION_DISABLE || action == AUTO_SKILL_ACTION_ENABLE)
		{
			uint32_t masteryId = 0;
			uint8_t flags = 0;
			if (!ReadBytes(pPacket, &masteryId, 4) || !ReadBytes(pPacket, &flags, 1))
				return;
			AppManager::LogMobKill("[AutoSkill] Char: '%s' (ID: %u) %s %s%s for mastery %u",
				inst->CharName16, inst->CharID, action ? "ENABLED" : "DISABLED",
				(flags & FLAG_MASTERY) ? "mastery " : "", (flags & FLAG_SKILL) ? "skills" : "", masteryId);
			return;
		}

		if (action != AUTO_SKILL_ACTION_TICK)
			return;

		uint8_t count = 0;
		if (!ReadBytes(pPacket, &count, 1) || count > 16)
			return;
		uint32_t ids[16];
		uint8_t flags[16];
		for (uint8_t i = 0; i < count; ++i)
		{
			if (!ReadBytes(pPacket, &ids[i], 4) || !ReadBytes(pPacket, &flags[i], 1))
				return;
		}

		std::lock_guard<std::mutex> lock(s_Mutex);

		DWORD now = GetTickCount();
		std::map<uint32_t, PlayerState>::iterator it = s_Players.find(inst->CharID);
		if (it == s_Players.end())
		{
			PlayerState st;
			st.lastHealth = inst->Health;
			st.lastDamageTick = now;   // first tick: wait one safe period before raising anything
			st.lastTick = 0;
			st.wasSafe = false;
			it = s_Players.insert(std::make_pair(inst->CharID, st)).first;
		}
		PlayerState& st = it->second;
		SampleHealth(st, inst);

		if (now - st.lastTick < 1000)
			return;   // one tick per second at most
		st.lastTick = now;

		if (!IsSafe(st, inst, safeSeconds))
		{
			st.wasSafe = false;
			return;
		}
		st.wasSafe = true;

		BuildSkillTable();

		uint32_t budget = maxLearnPerTick;
		uint32_t masteryUps = 0, skillUps = 0;
		char lastMastery[96] = "";

		// Masteries first: skills need the mastery level.
		for (uint8_t i = 0; i < count && budget > 0; ++i)
		{
			if (!(flags[i] & FLAG_MASTERY))
				continue;
			int newLevel = 0;
			while (budget > 0 && TryMasteryUp(pc, inst, ids[i], &newLevel))
			{
				--budget;
				++masteryUps;
				sprintf_s(lastMastery, "mastery %u -> %d", ids[i], newLevel);
			}
		}
		for (uint8_t i = 0; i < count && budget > 0; ++i)
		{
			if (!(flags[i] & FLAG_SKILL))
				continue;
			uint32_t got;
			while (budget > 0 && (got = SkillPass(pc, ids[i], budget)) > 0)
			{
				budget -= got;
				skillUps += got;
			}
		}

		if (masteryUps || skillUps)
		{
			AppManager::LogMobKill("[AutoSkill] Char: '%s' (ID: %u) +%u mastery level(s), +%u skill level(s) %s | SP left: %u",
				inst->CharName16, inst->CharID, masteryUps, skillUps, lastMastery, inst->RemainSkillPoint);
			if (chatNotice)
			{
				char msg[128];
				sprintf_s(msg, "Otomatik yukseltme: +%u mastery, +%u skill seviyesi. Kalan SP: %u",
					masteryUps, skillUps, inst->RemainSkillPoint);
				pc->SendChatMessage(6, "[Server]", msg);
			}
		}
	}
}

#include "AppManager.h"
// Console stuffs
#pragma warning(disable:4996) // _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "Dbghelp.lib")
#include <iostream>
// Thread signal
#include <csignal>
#include <sstream>
#include <fstream>
#include <exception>
// Gameserver stuffs
#include "Silkroad/CGObjManager.h"
// Utils
#include "Utils/IO/SimpleIni.h"
#include "Utils/Memory/Process.h"
#include "Utils/Memory/hook.h"
#include "Utils/StringHelpers.h"
#pragma warning(disable:4244) // Bitwise operations warnings
// ASM injection
#include "AsmEdition.h"

/// Static stuffs
bool AppManager::m_IsInitialized;
DatabaseLink AppManager::m_dbLink, AppManager::m_dbLinkHelper, AppManager::m_dbUniqueLog;
bool AppManager::m_IsRunningDatabaseFetch;
std::string AppManager::m_CTF_ITEM_WINNING_REWARD, AppManager::m_CTF_ITEM_KILLING_REWARD, AppManager::m_BA_ITEM_REWARD, AppManager::m_SPOOF_IP;
bool AppManager::m_MobKillStats_Enabled = false;
uint32_t AppManager::m_MobKillStats_RequiredCount = 100;
std::string AppManager::m_MobKillStats_RewardType = "STR";
uint16_t AppManager::m_MobKillStats_RewardAmount = 1;
bool AppManager::m_MobKillStats_LevelDiffCheck = true;
uint32_t AppManager::m_MobKillStats_MaxRewardCap = 50;
bool AppManager::m_MobKillStats_VerboseLog = true;
uint16_t AppManager::m_MobKillStats_MaxSafeStatLimit = 32000;
bool AppManager::m_MobKillStats_SendChatMessage = true;
uint8_t AppManager::m_MobKillStats_ChatType = 9;
std::string AppManager::m_MobKillStats_SenderName = "[Server]";
std::string AppManager::m_MobKillStats_ChatMessage = "Tebrikler! %u canavar kestiniz ve +%u %s kazandiniz! (Bonus: %u/%u)";
std::unordered_map<uint32_t, PlayerStatProgress> AppManager::m_MobKillTracker;
std::mutex AppManager::m_MobKillMutex;

void AppManager::Initialize()
{
	if (!m_IsInitialized)
	{
		m_IsInitialized = true;
		SetUnhandledExceptionFilter(&AppManager::CrashDumpFilter);
		InitConfigFile();
		InitDebugConsole();
		InitPatchValues();
		InitMobKillStats();
		InitHooks();
		InitDatabaseFetch();
	}
}
void AppManager::InitConfigFile()
{
	CSimpleIniA ini;
	// Try to load it or create a new one
	if (ini.LoadFile("vSRO-GameServer.ini") != SI_Error::SI_OK)
	{
		ini.SetSpaces(false);
		// Sql
		ini.SetValue("Sql", "HOST", "localhost", "; SQL Server address or name");
		ini.SetValue("Sql", "PORT", "1433", "; SQL Server port");
		ini.SetValue("Sql", "USER", "sa", "; Username credential");
		ini.SetValue("Sql", "PASS", "1234", "; Password credential");
		ini.SetValue("Sql", "DB_SHARD", "SRO_VT_SHARD", "; Name used for the specified silkroad database");
		ini.SetValue("Sql", "DB_SHARD_FETCH_TABLE", "_ExeGameServer", "; Table name used to execute actions");
		ini.SetValue("Sql", "DB_LOG", "SRO_VT_SHARDLOG", "; Name used for the specified silkroad database");
		// Memory
		ini.SetValue("Server", "SPOOF_IP", "auto", "; Certification IP override (auto = read from server.cfg, fixes Stud_PE header collision)");
		ini.SetLongValue("Server", "LEVEL_MAX", 110, "; Maximum level that can be reached on server");
		ini.SetLongValue("Server", "STALL_PRICE_LIMIT", 9999999999, "; Maximum price that can be stalled");
		ini.SetLongValue("Server", "PARTY_LEVEL_MIN", 5, "; Minimum level to create a party group");
		ini.SetLongValue("Server", "PARTY_MOB_MEMBERS_REQUIRED", 2, "; Party members required to find monsters party type");
		ini.SetLongValue("Server", "PARTY_MOB_SPAWN_PROBABILITY", 50, "; % Probability for party mob spawns");
		ini.SetLongValue("Server", "PK_LEVEL_REQUIRED", 20, "; Level required to kill other player");
		ini.SetLongValue("Server", "PENALTY_DROP_LEVEL_MIN", 10, "; Minimum level to apply drop penalty");
		ini.SetLongValue("Server", "PENALTY_DROP_PROBABILITY", 5, "; % Probability to drop an item when a player dies");
		ini.SetLongValue("Server", "RESURRECT_SAME_POINT_LEVEL_MAX", 10, "; Maximum level for resurrect at the same map position");
		ini.SetLongValue("Server", "NPC_RETURN_DEAD_LEVEL_MAX", 20, "; Maximum level for using \"Return to last dead point\" from NPC Guide");
		ini.SetLongValue("Server", "BEGINNER_MARK_LEVEL_MAX", 19, "; Maximum level to show the beginner mark");
		ini.SetLongValue("Server", "DROP_ITEM_MAGIC_PROBABILITY", 30, "; % Probability to drop items with blue options");
		ini.SetLongValue("Job", "LEVEL_MAX", 7, "; Maximum level that can be reached on job suit");
		ini.SetBoolValue("Job", "DISABLE_MOB_SPAWN", false, "; Disable Thief/Hunter monster spawn while trading");
		ini.SetLongValue("Job", "TEMPLE_LEVEL", 105, "; Minimum level to enter the Temple Area");
		ini.SetLongValue("Race", "CH_TOTAL_MASTERIES", 330, "; Masteries amount Chinese will obtain");
		ini.SetLongValue("Race", "EU_TOTAL_MASTERIES", 220, "; Masteries amount European will obtain");
		ini.SetLongValue("Guild", "MEMBERS_LIMIT_LEVEL1", 15, "; Guild members capacity at level 1");
		ini.SetLongValue("Guild", "MEMBERS_LIMIT_LEVEL2", 20, "; Guild members capacity at level 2");
		ini.SetLongValue("Guild", "MEMBERS_LIMIT_LEVEL3", 25, "; Guild members capacity at level 3");
		ini.SetLongValue("Guild", "MEMBERS_LIMIT_LEVEL4", 35, "; Guild members capacity at level 4");
		ini.SetLongValue("Guild", "MEMBERS_LIMIT_LEVEL5", 50, "; Guild members capacity at level 5");
		ini.SetLongValue("Guild", "STORAGE_SLOTS_MIN", 30, "; Storage slots at first level");
		ini.SetLongValue("Guild", "STORAGE_SLOTS_INCREASE", 30, "; Storage slots increased per level");
		ini.SetLongValue("Guild", "UNION_LIMIT", 8, "; Union participants limit");
		ini.SetLongValue("Guild", "UNION_CHAT_PARTICIPANTS", 12, "; Union chat participants allowed by guild");
		ini.SetLongValue("Academy", "DISBAND_PENALTY_TIME", 604800, "; Penalty time (seconds) to create again the group");
		ini.SetLongValue("Alchemy", "FUSING_DELAY", 3, "; Waiting delay (seconds) after fusing alchemy elements");
		ini.SetLongValue("Alchemy", "STONE_ASTRAL_VALUE", 4, "; Value from astral stones used to stop and prevent +0");
		ini.SetValue("Event", "CTF_ITEM_WIN_REWARD", "ITEM_ETC_E080723_ICETROPHY", "; Item reward from winning Capture The Flag");
		ini.SetLongValue("Event", "CTF_ITEM_WIN_REWARD_AMOUNT", 1, "; Amount to obtain by winning");
		ini.SetValue("Event", "CTF_ITEM_KILL_REWARD", "ITEM_ETC_E080723_ICETROPHY", "; Item reward from killing at Capture The Flag");
		ini.SetLongValue("Event", "CTF_ITEM_KILL_REWARD_AMOUNT", 1, "; Amount to obtain by kill");
		ini.SetValue("Event","BA_ITEM_REWARD","ITEM_ETC_ARENA_COIN","; Item reward from Battle Arena");
		ini.SetLongValue("Event", "BA_ITEM_REWARD_GJ_W_AMOUNT", 7, "; Amount to obtain winning on Guild/Job mode");
		ini.SetLongValue("Event", "BA_ITEM_REWARD_GJ_L_AMOUNT", 2, "; Amount to obtain loosing");
		ini.SetLongValue("Event", "BA_ITEM_REWARD_PR_W_AMOUNT", 5, "; Amount to obtain winning on Party/Random mode");
		ini.SetLongValue("Event", "BA_ITEM_REWARD_PR_L_AMOUNT", 1, "; Amount to obtain loosing");
		ini.SetLongValue("Fix", "AGENT_SERVER_CAPACITY", 1000, "; Set capacity supported by the connected agent server");
		ini.SetBoolValue("Fix", "HIGH_RATES_CONFIG", true, "; Fix rates (ExpRatio/1000) to use higher values than 2500");
		ini.SetBoolValue("Fix", "UNIQUE_LOGS", true, "; Log unique spawn/killed into _AddLogChar as EventID = 32/33");
		ini.SetBoolValue("Fix", "DISABLE_GREEN_BOOK", true, "; Disable buff with the green book");
		ini.SetBoolValue("Fix", "DISABLE_MSGBOX_SILK_GOLD_PRICE", true, "; Disable messages about \"register silk/gold price.\"");
		ini.SetBoolValue("Fix", "EXCHANGE_ATTACK_CANCEL", true, "; Remove attack cancel when player exchanges");
		ini.SetBoolValue("Fix", "EXPLOIT_INVISIBLE_INVINCIBLE", true, "; Cancel exploit sent from client (0x70A7)");
		ini.SetBoolValue("Fix", "GUILD_POINTS", true, "; Prevents negative values on guild points");
		// Mob Kill Stats
		ini.SetBoolValue("MobKillStats", "ENABLED", true, "; Enable mob kill stat reward system");
		ini.SetLongValue("MobKillStats", "MOBS_PER_REWARD", 100, "; Monsters required to get reward");
		ini.SetValue("MobKillStats", "REWARD_TYPE", "STR", "; Reward type: STR, INT, or STATPOINT");
		ini.SetLongValue("MobKillStats", "REWARD_AMOUNT", 1, "; Stat points awarded per milestone");
		ini.SetBoolValue("MobKillStats", "LEVEL_DIFF_CHECK", true, "; Only count mobs within player level range (+- 10)");
		ini.SetLongValue("MobKillStats", "MAX_REWARD_CAP", 50, "; Maximum total bonus stats a player can receive");
		ini.SetBoolValue("MobKillStats", "VERBOSE_LOG", true, "; Output verbose progress log on every kill (1=ON, 0=OFF)");
		ini.SetLongValue("MobKillStats", "MAX_SAFE_STAT_LIMIT", 32000, "; Safety clamp to prevent smallint (32767) DB & client overflow");
		ini.SetBoolValue("MobKillStats", "SEND_CHAT_MESSAGE", true, "; Send in-game chat message upon milestone achievement");
		ini.SetLongValue("MobKillStats", "CHAT_TYPE", 9, "; Chat type (9=PM/Pink Whisper, 2=Whisper, 1=General, 7=Notice)");
		ini.SetValue("MobKillStats", "CHAT_SENDER_NAME", "[Server]", "; Sender name for the message");
		ini.SetValue("MobKillStats", "CHAT_MESSAGE", "Tebrikler! %u canavar kestiniz ve +%u %s kazandiniz! (Bonus: %u/%u)", "; In-game message format template");
		// App
		ini.SetBoolValue("App", "DEBUG_CONSOLE", true, "; Attach debug console");
		// Save it
		ini.SaveFile("vSRO-GameServer.ini");
	}
}
void AppManager::InitMobKillStats()
{
	CSimpleIniA ini;
	ini.LoadFile("vSRO-GameServer.ini");

	m_MobKillStats_Enabled = ini.GetBoolValue("MobKillStats", "ENABLED", true);
	m_MobKillStats_RequiredCount = (uint32_t)ini.GetLongValue("MobKillStats", "MOBS_PER_REWARD", 100);
	m_MobKillStats_RewardType = ini.GetValue("MobKillStats", "REWARD_TYPE", "STR");
	m_MobKillStats_RewardAmount = (uint16_t)ini.GetLongValue("MobKillStats", "REWARD_AMOUNT", 1);
	m_MobKillStats_LevelDiffCheck = ini.GetBoolValue("MobKillStats", "LEVEL_DIFF_CHECK", true);
	m_MobKillStats_MaxRewardCap = (uint32_t)ini.GetLongValue("MobKillStats", "MAX_REWARD_CAP", 50);
	m_MobKillStats_VerboseLog = ini.GetBoolValue("MobKillStats", "VERBOSE_LOG", true);
	m_MobKillStats_MaxSafeStatLimit = (uint16_t)ini.GetLongValue("MobKillStats", "MAX_SAFE_STAT_LIMIT", 32000);
	m_MobKillStats_SendChatMessage = ini.GetBoolValue("MobKillStats", "SEND_CHAT_MESSAGE", true);
	m_MobKillStats_ChatType = (uint8_t)ini.GetLongValue("MobKillStats", "CHAT_TYPE", 9);
	m_MobKillStats_SenderName = ini.GetValue("MobKillStats", "CHAT_SENDER_NAME", "[Server]");
	m_MobKillStats_ChatMessage = ini.GetValue("MobKillStats", "CHAT_MESSAGE", "Tebrikler! %u canavar kestiniz ve +%u %s kazandiniz! (Bonus: %u/%u)");

	if (m_MobKillStats_Enabled)
	{
		LogMobKill("[MobKillStats] ENABLED | %u Mobs -> +%u %s (Cap: %u, LevelDiffCheck: %s, MaxSafeStat: %u, Chat: %s, ChatType: %u)",
			m_MobKillStats_RequiredCount,
			m_MobKillStats_RewardAmount,
			m_MobKillStats_RewardType.c_str(),
			m_MobKillStats_MaxRewardCap,
			m_MobKillStats_LevelDiffCheck ? "ON" : "OFF",
			m_MobKillStats_MaxSafeStatLimit,
			m_MobKillStats_SendChatMessage ? "ON" : "OFF",
			m_MobKillStats_ChatType);
	}
	else
	{
		LogMobKill("[MobKillStats] DISABLED");
	}
}
void AppManager::InitDebugConsole()
{
	// Load file
	CSimpleIniA ini;
	ini.LoadFile("vSRO-GameServer.ini");

	// Check if console has been deactivated
	if (ini.GetBoolValue("App", "DEBUG_CONSOLE", true))
	{
		AllocConsole();
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
		freopen("CONIN$", "r", stdin);
	}
}
static bool placeHook6(uintptr_t hookAddr, void* targetFunc)
{
	DWORD dwProtect = 0;
	if (!VirtualProtect((LPVOID)hookAddr, 6, PAGE_EXECUTE_READWRITE, &dwProtect))
		return false;

	uint8_t bytes[6];
	bytes[0] = 0xE9; // JMP rel32
	int32_t rel = (int32_t)((uintptr_t)targetFunc - (hookAddr + 5));
	memcpy(&bytes[1], &rel, 4);
	bytes[5] = 0x90; // NOP padding

	memcpy((LPVOID)hookAddr, bytes, 6);
	VirtualProtect((LPVOID)hookAddr, 6, dwProtect, &dwProtect);
	FlushInstructionCache(GetCurrentProcess(), (LPCVOID)hookAddr, 6);
	return true;
}

void __declspec(naked) Hook_OnCharDie()
{
	__asm
	{
		pushad
		pushfd

		// At entry of 0x004C3F60 (CGObjChar::OnDie):
		// ecx = pDeadEntity (this)
		// [esp + 4] = pAttacker (pKiller)
		// After pushad (32 bytes) + pushfd (4 bytes) + return addr (4 bytes) = 40 bytes:
		mov edx, [esp + 40]
		push ecx            // arg2: pVictim (dead mob/entity)
		push edx            // arg1: pAttacker (killer PC/pet)
		call AppManager::OnUniversalKillRewardBridge

		popfd
		popad

		// Original 6 bytes overwritten at 0x004C3F60:
		// 004c3f60: 55              push    ebp
		// 004c3f61: 8bec            mov     ebp,esp
		// 004c3f63: 83e4f8          and     esp,0FFFFFFF8h
		push ebp
		mov ebp, esp
		and esp, 0FFFFFFF8h

		// Jump back to 0x004C3F66
		mov edx, 0x004C3F66
		jmp edx
	}
}

void AppManager::InitHooks()
{
	std::cout << " * Initializing hooks..." << std::endl;

	// Mob Kill Stats Hooks
	if (m_MobKillStats_Enabled)
	{
		printf(" - HOOK_MOB_KILL_STATS (CGObjChar::OnDie 0x004C3F60)\r\n");
		bool hookDeath = placeHook6(0x004C3F60, Hook_OnCharDie);
		if (hookDeath)
		{
			LogMobKill("[MobKillStats] Universal CGObjChar::OnDie Hook Registered Successfully! (0x004C3F60)");
		}
		else
		{
			LogMobKill("[MobKillStats ERROR] Failed to register hook at 0x004C3F60!");
		}
	}

	// Load file
	CSimpleIniA ini;
	ini.LoadFile("vSRO-GameServer.ini");

	// Fixes
	if (ini.GetBoolValue("Fix","UNIQUE_LOGS",true))
	{
		// Create connection string
		std::wstringstream connString;
		connString << "DRIVER={SQL Server};";
		connString << "SERVER=" << ini.GetValue("Sql", "HOST", "localhost") << ", " << ini.GetValue("Sql", "PORT", "1433") << ";";
		connString << "DATABASE=" << ini.GetValue("Sql", "DB_LOG", "SRO_VT_SHARDLOG") << ";";
		connString << "UID=" << ini.GetValue("Sql", "USER", "sa") << ";";
		connString << "PWD=" << ini.GetValue("Sql", "PASS", "1234") << ";";

		if (m_dbUniqueLog.sqlConn.Open((SQLWCHAR*)connString.str().c_str()) && m_dbUniqueLog.sqlCmd.Open(m_dbUniqueLog.sqlConn))
		{
			printf(" - FIX_UNIQUE_LOGS\r\n");
			if (replaceOffset(0x00414DB0, addr_from_this(&AppManager::OnUniqueSpawnMsg)))
			{
				std::cout << "   - OnUniqueSpawnMsg" << std::endl;
			}
			if (replaceOffset(0x00414BA9, addr_from_this(&AppManager::OnUniqueKilledMsg)))
			{
				std::cout << "   - OnUniqueKilledMsg" << std::endl;
			}
		}
	}
	if (ini.GetBoolValue("Fix", "GUILD_POINTS", true))
	{
		printf(" - FIX_GUILD_POINTS\r\n");
		// Redirect code flow to DLL
		if (placeHook(0x005C4135, addr_from_this(&AsmEdition::OnDonateGuildPoints)))
		{
			std::cout << "   - OnDonateGuildPoints" << std::endl;
		}
	}
}
void __stdcall AppManager::OnEntityKilled(void* pAttacker, void* pDeadEntity)
{
	ProcessMobKillReward(pAttacker, pDeadEntity);
	__asm xor ecx, ecx;
	reinterpret_cast<void(__stdcall*)(void*, void*)>(0x0049F590)(pAttacker, pDeadEntity);
}

void __stdcall AppManager::OnUniversalKillRewardBridge(void* pAttacker, void* pVictim)
{
	ProcessMobKillReward(pAttacker, pVictim);
}

void __stdcall AppManager::OnEntityKilledParty(void* pAttacker, void* pDeadEntity)
{
	ProcessMobKillReward(pAttacker, pDeadEntity);
	void* pAtt = pAttacker;
	__asm mov ecx, pAtt;
	reinterpret_cast<void(__stdcall*)(void*, void*)>(0x0049F590)(pAttacker, pDeadEntity);
}

void AppManager::LogMobKill(const char* format, ...)
{
	static std::mutex s_LogMutex;
	std::lock_guard<std::mutex> lock(s_LogMutex);

	char buffer[2048];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	SYSTEMTIME st;
	GetLocalTime(&st);
	char timeStr[64];
	sprintf_s(timeStr, "[%04d-%02d-%02d %02d:%02d:%02d.%03d]",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	printf(" * %s %s\r\n", timeStr, buffer);

	FILE* f = nullptr;
	if (fopen_s(&f, "vSRO-MobKillStats.log", "a") == 0 && f)
	{
		fprintf(f, "%s %s\n", timeStr, buffer);
		fflush(f);
		fclose(f);
	}
}

LONG WINAPI AppManager::CrashDumpFilter(EXCEPTION_POINTERS* pExceptionPointers)
{
	SYSTEMTIME st;
	GetLocalTime(&st);
	char dumpPath[MAX_PATH];
	sprintf_s(dumpPath, "Crash_GameServer_%04d%02d%02d_%02d%02d%02d.dmp",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

	HANDLE hFile = CreateFileA(dumpPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		MINIDUMP_EXCEPTION_INFORMATION mei;
		mei.ThreadId = GetCurrentThreadId();
		mei.ExceptionPointers = pExceptionPointers;
		mei.ClientPointers = FALSE;

		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpWithFullMemoryInfo, &mei, NULL, NULL);
		CloseHandle(hFile);
	}

	LogMobKill("[FATAL CRASH] Unhandled exception in GameServer! MiniDump written to %s. ExceptionCode: 0x%08X at 0x%p",
		dumpPath,
		pExceptionPointers->ExceptionRecord->ExceptionCode,
		pExceptionPointers->ExceptionRecord->ExceptionAddress);

	return EXCEPTION_CONTINUE_SEARCH;
}

uint16_t AppManager::SafeAddStat(uint16_t currentVal, uint16_t rewardAmount, uint16_t maxLimit, const char* statName, const char* charName)
{
	if (currentVal >= maxLimit)
	{
		LogMobKill("[MobKillStats GUARD] Player '%s' stat '%s' is already at/above safe limit (%u >= %u). Addition skipped to prevent smallint overflow.",
			charName, statName, currentVal, maxLimit);
		return currentVal;
	}

	uint32_t sum = static_cast<uint32_t>(currentVal) + static_cast<uint32_t>(rewardAmount);
	if (sum > maxLimit)
	{
		LogMobKill("[MobKillStats GUARD] Player '%s' stat '%s' clamped from %u to %u (safe limit: %u) to prevent smallint overflow.",
			charName, statName, sum, maxLimit, maxLimit);
		return maxLimit;
	}

	return static_cast<uint16_t>(sum);
}

void AppManager::ProcessMobKillReward(void* pAttacker, void* pDeadEntity)
{
	if (!m_MobKillStats_Enabled)
		return;

	__try
	{
		ProcessMobKillRewardInternal(pAttacker, pDeadEntity);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		DWORD code = GetExceptionCode();
		LogMobKill("[CRITICAL SEH EXCEPTION] Hardware/Access violation prevented! ExceptionCode: 0x%08X", code);
	}
}

void AppManager::ProcessMobKillRewardInternal(void* pAttacker, void* pDeadEntity)
{
	try
	{
		if (!pAttacker || !pDeadEntity)
			return;

		CGObj* pAttackerObj = reinterpret_cast<CGObj*>(pAttacker);
		CGObj* pDeadObj = reinterpret_cast<CGObj*>(pDeadEntity);

		uint32_t attackerVFTable = pAttackerObj->GetVFTable();
		uint32_t deadVFTable = pDeadObj->GetVFTable();

		if (m_MobKillStats_VerboseLog)
			LogMobKill("[MobKillStats Event] OnDie: Attacker=0x%p (VFT=0x%08X), Dead=0x%p (VFT=0x%08X)",
				pAttacker, attackerVFTable, pDeadEntity, deadVFTable);

		// Check if the dead entity is a Monster (CGObjMob vftables: 0x00AEDDAC, 0x00AEE3F0, or 0x00AEE3E4)
		if (deadVFTable != 0x00AEDDAC && deadVFTable != 0x00AEE3F0 && deadVFTable != 0x00AEE3E4)
		{
			if (m_MobKillStats_VerboseLog)
				LogMobKill("[MobKillStats Skip] Victim 0x%p is not a monster (VFTable: 0x%08X)", pDeadEntity, deadVFTable);
			return;
		}

		// If attacker is not a player directly, check if it's a pet/summon owned by a player
		if (attackerVFTable != 0x00AF59FC && attackerVFTable != 0x00AF60AC)
		{
			void* pOwner = *reinterpret_cast<void**>((uintptr_t)pAttacker + 0x14C);
			if (pOwner)
			{
				uint32_t ownerVFTable = *reinterpret_cast<uint32_t*>(pOwner);
				if (ownerVFTable == 0x00AF59FC || ownerVFTable == 0x00AF60AC)
				{
					if (m_MobKillStats_VerboseLog)
						LogMobKill("[MobKillStats Pet] Attacker 0x%p resolved to owner 0x%p", pAttacker, pOwner);
					pAttacker = pOwner;
					attackerVFTable = ownerVFTable;
				}
			}
		}

		// Check if the attacker is a Player (CGObjPC vftables: 0x00AF59FC or 0x00AF60AC)
		if (attackerVFTable != 0x00AF59FC && attackerVFTable != 0x00AF60AC)
		{
			if (m_MobKillStats_VerboseLog)
				LogMobKill("[MobKillStats Skip] Attacker 0x%p is not a player (VFTable: 0x%08X)", pAttacker, attackerVFTable);
			return;
		}

		CGObjPC* pPlayer = reinterpret_cast<CGObjPC*>(pAttacker);
		CInstancePC* pInstance = pPlayer->GetInstance();
		if (!pInstance)
		{
			LogMobKill("[MobKillStats ERROR] Player instance pointer is null!");
			return;
		}

		uint32_t charId = pPlayer->GetCharID();
		const char* charName = pInstance->CharName16;

		// Level difference check (Anti-Abuse)
		if (m_MobKillStats_LevelDiffCheck)
		{
			uint8_t mobLevel = *reinterpret_cast<uint8_t*>((uintptr_t)pDeadEntity + 0xE0);
			uint8_t playerLevel = *reinterpret_cast<uint8_t*>((uintptr_t)pPlayer + 0xE0);

			if (mobLevel > 0 && playerLevel > 0)
			{
				int levelDiff = (int)playerLevel - (int)mobLevel;
				if (levelDiff > 10)
				{
					if (m_MobKillStats_VerboseLog)
						LogMobKill("[MobKillStats Skip] Player '%s' (Lv %u) vs Mob (Lv %u) = Diff %d > 10", charName, playerLevel, mobLevel, levelDiff);
					return; // Skipped low-level mob kill
				}
			}
		}

		std::lock_guard<std::mutex> lock(m_MobKillMutex);
		auto& progress = m_MobKillTracker[charId];
		progress.currentKills++;

		if (m_MobKillStats_VerboseLog && progress.currentKills < m_MobKillStats_RequiredCount)
		{
			LogMobKill("[MobKillStats Progress] Player '%s' (ID: %u) killed monster. Progress: %u / %u",
				charName, charId, progress.currentKills, m_MobKillStats_RequiredCount);
		}

		if (progress.currentKills >= m_MobKillStats_RequiredCount)
		{
			if (progress.bonusGranted >= m_MobKillStats_MaxRewardCap)
			{
				LogMobKill("[MobKillStats Info] Player '%s' reached max bonus cap (%u/%u).",
					charName, progress.bonusGranted, m_MobKillStats_MaxRewardCap);
				progress.currentKills = 0;
				return;
			}

			uint16_t awarded = 0;
			if (m_MobKillStats_RewardType == "STR")
			{
				uint16_t prev = pInstance->Strength;
				pInstance->Strength = SafeAddStat(prev, m_MobKillStats_RewardAmount, m_MobKillStats_MaxSafeStatLimit, "STR", charName);
				awarded = pInstance->Strength - prev;
				if (awarded > 0)
				{
					*reinterpret_cast<uint32_t*>((uintptr_t)pInstance + 0x08) |= 0x04; // DB dirty flag for STR/INT
					pPlayer->SendStatUpdate(); // 0x303D combat stat sync
				}
			}
			else if (m_MobKillStats_RewardType == "INT")
			{
				uint16_t prev = pInstance->Intelect;
				pInstance->Intelect = SafeAddStat(prev, m_MobKillStats_RewardAmount, m_MobKillStats_MaxSafeStatLimit, "INT", charName);
				awarded = pInstance->Intelect - prev;
				if (awarded > 0)
				{
					*reinterpret_cast<uint32_t*>((uintptr_t)pInstance + 0x08) |= 0x04; // DB dirty flag for STR/INT
					pPlayer->SendStatUpdate(); // 0x303D combat stat sync
				}
			}
			else // STATPOINT
			{
				uint16_t prev = pInstance->RemainStatPoint;
				pInstance->RemainStatPoint = SafeAddStat(prev, m_MobKillStats_RewardAmount, m_MobKillStats_MaxSafeStatLimit, "RemainStatPoint", charName);
				awarded = pInstance->RemainStatPoint - prev;
				if (awarded > 0)
				{
					*reinterpret_cast<uint32_t*>((uintptr_t)pInstance + 0x08) |= 0x10; // DB dirty flag for StatPoint
					pPlayer->SendStuffUpdate(3, false); // 0x304E real-time stat point sync to client
					pPlayer->SendStatUpdate(); // 0x303D combat stat sync
				}
			}

			progress.bonusGranted += awarded;
			progress.currentKills = 0;

			// Send In-Game Chat Message (Pink Whisper)
			if (m_MobKillStats_SendChatMessage && awarded > 0)
			{
				char msgBuffer[256];
				if (strstr(m_MobKillStats_ChatMessage.c_str(), "%") != nullptr)
				{
					snprintf(msgBuffer, sizeof(msgBuffer), m_MobKillStats_ChatMessage.c_str(),
						m_MobKillStats_RequiredCount,
						awarded,
						m_MobKillStats_RewardType.c_str(),
						progress.bonusGranted,
						m_MobKillStats_MaxRewardCap);
				}
				else
				{
					strncpy(msgBuffer, m_MobKillStats_ChatMessage.c_str(), sizeof(msgBuffer) - 1);
					msgBuffer[sizeof(msgBuffer) - 1] = '\0';
				}

				pPlayer->SendChatMessage(m_MobKillStats_ChatType, m_MobKillStats_SenderName.c_str(), msgBuffer);
			}

			LogMobKill("[MobKillStats SUCCESS] Milestone Reached! Player: '%s' (ID: %u) -> Rewarded: +%u %s | Current Stats: [STR: %u | INT: %u | StatPoints: %u] | Bonus: %u/%u | Client Sync Dispatched! | Chat: %s",
				charName, charId,
				awarded, m_MobKillStats_RewardType.c_str(),
				pInstance->Strength, pInstance->Intelect, pInstance->RemainStatPoint,
				progress.bonusGranted, m_MobKillStats_MaxRewardCap,
				(m_MobKillStats_SendChatMessage && awarded > 0) ? "SENT" : "OFF");
		}
	}
	catch (const std::exception& ex)
	{
		LogMobKill("[MobKillStats EXCEPTION] Exception in ProcessMobKillRewardInternal: %s", ex.what());
	}
	catch (...)
	{
		LogMobKill("[MobKillStats CRITICAL] Unknown exception in ProcessMobKillRewardInternal!");
	}
}

void AppManager::OnUniqueSpawnMsg(uint32_t LogType, const char* Message, const char* UniqueCodeName, uint16_t RegionId, uint32_t unk01, uint32_t unk02)
{
	// Build query
	std::wstringstream qExecLog;
	qExecLog << "EXEC dbo._AddLogChar";
	qExecLog << " 0,32," << RegionId << ",0,'','" << UniqueCodeName << "'";

	// Try execute it
	m_dbUniqueLog.sqlCmd.ExecuteQuery((SQLWCHAR*)qExecLog.str().c_str());
	m_dbUniqueLog.sqlCmd.Clear();

	// Avoid call original function since it's about printing into gameserver logs
}
void AppManager::OnUniqueKilledMsg(uint32_t LogType, const char* Message, const char* UniqueCodeName, const char* CharName)
{
	auto player = CGObjManager::GetObjPCByCharName16(CharName);
	auto pos = player->GetPosition();
	// Build query
	std::wstringstream qExecLog;
	qExecLog << "EXEC dbo._AddLogChar";
	qExecLog << " " << player->GetCharID() << ",33," << pos.RegionId << "," << pos.UnkUShort01 << ",'" << (int)pos.PosX << "," << (int)pos.PosY << "," << (int)pos.PosZ << "','" << UniqueCodeName << "'";

	// Try execute it
	m_dbUniqueLog.sqlCmd.ExecuteQuery((SQLWCHAR*)qExecLog.str().c_str());
	m_dbUniqueLog.sqlCmd.Clear();

	// Avoid call original function since it's about printing into gameserver logs
}
void AppManager::InitPatchValues()
{
	std::cout << " * Initializing patches..." << std::endl;

	// Load file
	CSimpleIniA ini;
	ini.LoadFile("vSRO-GameServer.ini");

	// buffers
	uint8_t byteValue;
	uint32_t uintValue;
	double_t doubleValue;

	// Server
	// Fix Stud_PE 5th section header (0x00400288) overwriting _IPSpoofing.exe Certification IP
	{
		uint8_t pushOp1 = 0, pushOp2 = 0;
		if (ReadMemoryValue<uint8_t>(0x009365B9, pushOp1) && ReadMemoryValue<uint8_t>(0x0093660D, pushOp2)
			&& pushOp1 == 0x68 && pushOp2 == 0x68)
		{
			std::string configuredIp = ini.GetValue("Server", "SPOOF_IP", "auto");
			if (configuredIp.empty() || _stricmp(configuredIp.c_str(), "auto") == 0)
			{
				configuredIp.clear();
				std::ifstream cfg("server.cfg");
				if (cfg.is_open())
				{
					std::string line;
					bool inGameServerBlock = false;
					int braceDepth = 0;
					while (std::getline(cfg, line))
					{
						size_t commentPos = line.find("//");
						if (commentPos != std::string::npos)
							line = line.substr(0, commentPos);

						if (!inGameServerBlock && braceDepth == 0)
						{
							size_t gsPos = line.find("SR_GameServer");
							if (gsPos != std::string::npos)
							{
								inGameServerBlock = true;
							}
						}

						for (char ch : line)
						{
							if (ch == '{') braceDepth++;
							else if (ch == '}') braceDepth--;
						}

						if (inGameServerBlock)
						{
							size_t certPos = line.find("Certification");
							if (certPos == std::string::npos)
								certPos = line.find("certification");

							if (certPos != std::string::npos)
							{
								size_t q1 = line.find('"', certPos);
								size_t q2 = (q1 != std::string::npos) ? line.find('"', q1 + 1) : std::string::npos;
								if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1 + 1)
								{
									configuredIp = line.substr(q1 + 1, q2 - q1 - 1);
									break;
								}
							}

							if (braceDepth <= 0 && line.find('}') != std::string::npos)
								inGameServerBlock = false;
						}
					}
				}
				if (configuredIp.empty() || configuredIp.size() >= 16)
					configuredIp = "%d.%d.%d.%d";
			}

			if (!configuredIp.empty() && configuredIp.size() < 16)
			{
				m_SPOOF_IP = configuredIp;
				WriteMemoryValue<uint32_t>(0x009365BA, (uint32_t)m_SPOOF_IP.c_str());
				WriteMemoryValue<uint32_t>(0x0093660E, (uint32_t)m_SPOOF_IP.c_str());
				printf(" - SERVER_SPOOF_IP -> (%s) [Fixed Stud_PE 0x00400288 collision]\r\n", m_SPOOF_IP.c_str());
				LogMobKill("[Fix] Restored SR_GameServer Certification IP -> %s (0x009365BA & 0x0093660E)", m_SPOOF_IP.c_str());
			}
		}
	}
	if (ReadMemoryValue<uint8_t>(0x004E52C7 + 2, byteValue))
	{
		uint8_t newValue = ini.GetLongValue("Server", "LEVEL_MAX", 110);
		printf(" - SERVER_LEVEL_MAX (%d) -> (%d)\r\n", byteValue, newValue);
		WriteMemoryValue<uint8_t>(0x004E52C7 + 2, newValue); // Character
		WriteMemoryValue<uint8_t>(0x004D641B + 3, newValue); // Pet
		WriteMemoryValue<uint16_t>(0x004E5471 + 4, (newValue - 1) * 4); // Exp bug fix
	}
	if (ReadMemoryValue<uint8_t>(0x00471B00 + 2, byteValue) && ReadMemoryValue<uint32_t>(0x00471B07 + 1, uintValue))
	{
		uint64_t newValue = ini.GetLongValue("Server", "STALL_PRICE_LIMIT", 9999999999);
		newValue = newValue & 0xFFFFFFFFFF; // Limit value to 5 bytes
		printf(" - SERVER_STALL_PRICE_LIMIT (%llu) -> (%llu)\r\n", ((uint64_t)byteValue << 32) | uintValue, newValue);
		// Stall
		WriteMemoryValue<uint8_t>(0x00471B00 + 2, newValue >> 32);
		WriteMemoryValue<uint32_t>(0x00471B07 + 1, newValue);
		WriteMemoryValue<uint8_t>(0x00472FF5 + 2, newValue >> 32);
		WriteMemoryValue<uint32_t>(0x00473008 + 1, newValue);
		WriteMemoryValue<uint8_t>(0x0047ABD8 + 2, newValue >> 32);
		WriteMemoryValue<uint32_t>(0x0047ABE3 + 1, newValue);
		// Exchange will take the highest UX value
		if (ReadMemoryValue<uint32_t>(0x00480F5E + 4, uintValue))
		{
			if (newValue > 4000000000u)
				newValue = 4000000000u;
			printf(" - SERVER_EXCHANGE_GOLD_LIMIT (%u) -> (%u)\r\n", uintValue, (uint32_t)newValue);
			WriteMemoryValue<uint32_t>(0x00480F5E + 4, newValue);
			WriteMemoryValue<uint32_t>(0x004D8F1A + 2, newValue);
			WriteMemoryValue<uint32_t>(0x004D8F22 + 1, newValue); // Fixed offset: opcode BF (mov edi, imm32) is 1 byte (+1); +2 was clobbering following EB (jmp)
			WriteMemoryValue<uint32_t>(0x004F7734 + 2, newValue);
			WriteMemoryValue<uint32_t>(0x004F7746 + 4, newValue);
		}
	}
	if(ReadMemoryValue<uint8_t>(0x00513FEC + 1, byteValue))
	{
		uint8_t newValue = ini.GetLongValue("Server", "PARTY_LEVEL_MIN", 5);
		printf(" - SERVER_PARTY_LEVEL_MIN (%d) -> (%d)\r\n", byteValue, newValue);
		WriteMemoryValue<uint8_t>(0x00513FEC + 1, newValue);
	}
	if(ReadMemoryValue<uint8_t>(0x00558F20 + 4, byteValue))
	{
		uint8_t newValue = ini.GetLongValue("Server", "PARTY_MOB_MEMBERS_REQUIRED", 2);
		printf(" - SERVER_PARTY_MOB_MEMBERS_REQUIRED (%d) -> (%d)\r\n", byteValue, newValue);
		WriteMemoryValue<uint8_t>(0x00558F20 + 4, newValue);
	}
	if (ReadMemoryValue<uint8_t>(0x005608E2 + 2, byteValue))
	{
		uint8_t newValue = ini.GetLongValue("Server", "PARTY_MOB_SPAWN_PROBABILITY", 50);
		printf(" - SERVER_PARTY_MOB_SPAWN_PROBABILITY (%d) -> (%d)\r\n", byteValue, newValue);
		WriteMemoryValue<uint8_t>(0x005608E2 + 2, newValue);
	}
	if(ReadMemoryValue<uint8_t>(0x005295DA + 1, byteValue))
	{
		uint8_t newValue = ini.GetLongValue("Server", "PK_LEVEL_REQUIRED", 20);
		printf(" - SERVER_PK_LEVEL_REQUIRED (%d) -> (%d)\r\n", byteValue, newValue);
		WriteMemoryValue<uint8_t>(0x005295DA + 1, newValue);
	}
	if (ReadMemoryValue<uint8_t>(0x004E6A33 + 1, byteValue))
	{
		uint8_t newValue = ini.GetLongValue("Server", "PENALTY_DROP_LEVEL_MIN", 10);
		printf(" - SERVER_PENALTY_DROP_LEVEL_MIN (%d) -> (%d)\r\n", byteValue, newValue);
		WriteMemoryValue<uint8_t>(0x004E6A33 + 1, newValue);
	}
	if (ReadMemoryValue<uint8_t>(0x004E696D + 1, byteValue))
	{
		uint8_t newValue = ini.GetLongValue("Server", "PENALTY_DROP_PROBABILITY", 5);
		printf(" - SERVER_PENALTY_DROP_PROBABILITY (%d) -> (%d)\r\n", byteValue, newValue);
		WriteMemoryValue<uint8_t>(0x004E696D + 1, newValue);
	}
	if (ReadMemoryValue<uint8_t>(0x0051017F + 1, byteValue))
	{
		uint8_t newValue = ini.GetLongValue("Server", "RESURRECT_SAME_POINT_LEVEL_MAX", 10);
		printf(" - SERVER_RESURRECT_SAME_POINT_LEVEL_MAX (%d) -> (%d)\r\n", byteValue, newValue);
		WriteMemoryValue<uint8_t>(0x0051017F + 1, newValue);
	}
	if (ReadMemoryValue<uint8_t>(0x004F36F3 + 1, byteValue))
	{
		uint8_t newValue = ini.GetLongValue("Server", "NPC_RETURN_DEAD_LEVEL_MAX", 20);
		printf(" - SERVER_NPC_RETURN_DEAD_LEVEL_MAX (%d) -> (%d)\r\n", byteValue, newValue);
		WriteMemoryValue<uint8_t>(0x004F36F3 + 1, newValue);
	}
	if (ReadMemoryValue<uint8_t>(0x004E4F0F + 4, byteValue))
	{
		uint8_t newValue = ini.GetLongValue("Server", "BEGINNER_MARK_LEVEL_MAX", 19);
		printf(" - SERVER_BEGINNER_MARK_LEVEL_MAX (%d) -> (%d)\r\n", byteValue, newValue);
		WriteMemoryValue<uint8_t>(0x004E4F0F + 4, newValue);
		WriteMemoryValue<uint8_t>(0x00518B99 + 3, newValue);
	}
	if (ReadMemoryValue<uint8_t>(0x00727784 + 2, byteValue))
	{
		uint8_t newValue = ini.GetLongValue("Server", "DROP_ITEM_MAGIC_PROBABILITY", 30);
		printf(" - SERVER_DROP_ITEM_MAGIC_PROBABILITY (%d) -> (%d)\r\n", byteValue, newValue);
		WriteMemoryValue<uint8_t>(0x00727784 + 2, newValue);
	}

	// Job
	if (ReadMemoryValue<uint8_t>(0x0060DE69 + 3, byteValue))
	{
		uint8_t newValue = ini.GetLongValue("Job", "LEVEL_MAX", 7);
		printf(" - JOB_LEVEL_MAX (%d) -> (%d)\r\n", byteValue, newValue);
		WriteMemoryValue<uint8_t>(0x0060DE69 + 3, newValue);
	}
	if (ini.GetBoolValue("Job", "DISABLE_MOB_SPAWN", false))
	{
		printf(" - JOB_DISABLE_MOB_SPAWN\r\n");
		WriteMemoryValue<uint16_t>(0x0060C4AB, 0xC031); // mov eax,esi -> xor eax,eax
	}
	if (ReadMemoryValue<uint8_t>(0x0051AE71 + 1, byteValue))
	{
		uint8_t newValue = ini.GetLongValue("Job", "TEMPLE_LEVEL", 105);
		printf(" - JOB_TEMPLE_LEVEL (%d) -> (%d)\r\n", byteValue, newValue);
		WriteMemoryValue<uint8_t>(0x0051AE71 + 1, newValue);
		WriteMemoryValue<uint8_t>(0x0051ABE8 + 1, newValue);
	}

	// Race
	if (ReadMemoryValue<uint32_t>(0x0059C5E6 + 1, uintValue))
	{
		uint32_t newValue = ini.GetLongValue("Race", "CH_TOTAL_MASTERIES", 330);
		printf(" - RACE_CH_TOTAL_MASTERIES (%u) -> (%u)\r\n", uintValue, newValue);
		WriteMemoryValue<uint32_t>(0x0059C5E6 + 1, newValue);
	}
	if (ReadMemoryValue<double_t>(0x00B46130, doubleValue))
	{
		uint32_t newValue = ini.GetLongValue("Race", "EU_TOTAL_MASTERIES", 220);
		printf(" - RACE_EU_TOTAL_MASTERIES (%u) -> (%u)\r\n", (uint32_t)doubleValue, newValue);
		WriteMemoryValue<double_t>(0x00B46130, (double_t)newValue);
	}

	// Guild
	{
		uint32_t addr = 0x00ADE8DC;
		if (ReadMemoryValue<uint32_t>(addr, uintValue))
		{
			uint32_t newValue = ini.GetLongValue("Guild", "MEMBERS_LIMIT_LEVEL1", 15);
			printf(" - GUILD_MEMBERS_LIMIT_LEVEL1 (%u) -> (%u)\r\n", uintValue, newValue);
			WriteMemoryValue<uint32_t>(addr, newValue);
		}
		if (ReadMemoryValue<uint32_t>(addr + 4, uintValue))
		{
			uint32_t newValue = ini.GetLongValue("Guild", "MEMBERS_LIMIT_LEVEL2", 20);
			printf(" - GUILD_MEMBERS_LIMIT_LEVEL2 (%u) -> (%u)\r\n", uintValue, newValue);
			WriteMemoryValue<uint32_t>(addr + 4, newValue);
		}
		if (ReadMemoryValue<uint32_t>(addr + 8, uintValue))
		{
			uint32_t newValue = ini.GetLongValue("Guild", "MEMBERS_LIMIT_LEVEL3", 25);
			printf(" - GUILD_MEMBERS_LIMIT_LEVEL3 (%u) -> (%u)\r\n", uintValue, newValue);
			WriteMemoryValue<uint32_t>(addr + 8, newValue);
		}
		if (ReadMemoryValue<uint32_t>(addr + 12, uintValue))
		{
			uint32_t newValue = ini.GetLongValue("Guild", "MEMBERS_LIMIT_LEVEL4", 35);
			printf(" - GUILD_MEMBERS_LIMIT_LEVEL4 (%u) -> (%u)\r\n", uintValue, newValue);
			WriteMemoryValue<uint32_t>(addr + 12, newValue);
		}
		if (ReadMemoryValue<uint32_t>(addr + 16, uintValue))
		{
			uint32_t newValue = ini.GetLongValue("Guild", "MEMBERS_LIMIT_LEVEL5", 50);
			printf(" - GUILD_MEMBERS_LIMIT_LEVEL5 (%u) -> (%u)\r\n", uintValue, newValue);
			WriteMemoryValue<uint32_t>(addr + 16, newValue);
		}
	}
	if (ReadMemoryValue<uint32_t>(0x00C6B5F8, uintValue))
	{
		uint32_t newValue = ini.GetLongValue("Guild", "STORAGE_SLOTS_MIN", 30);
		printf(" - GUILD_STORAGE_SLOTS_MIN (%d) -> (%d)\r\n", uintValue, newValue);
		WriteMemoryValue<uint32_t>(0x00C6B5F8, newValue);
		// Get value increased on second level
		uint32_t increaseValue;
		if (ReadMemoryValue<uint32_t>(0x00C6B5F8 + 4, increaseValue))
		{
			uint32_t increaseNewValue = ini.GetLongValue("Guild", "STORAGE_SLOTS_INCREASE", 30);
			printf(" - GUILD_STORAGE_SLOTS_INCREASE (%d) -> (%d)\r\n", increaseValue-uintValue, increaseNewValue);
			for(int i = 0; i < 3; i++)
				WriteMemoryValue<uint32_t>(0x00C6B5F8 + 4 + (i*4), newValue + (i+1)*increaseNewValue);
		}
	}
	if (ReadMemoryValue<uint8_t>(0x005B8EA1 + 1, byteValue))
	{
		uint8_t newValue = ini.GetLongValue("Guild", "UNION_LIMIT", 8);
		printf(" - GUILD_UNION_LIMIT (%d) -> (%d)\r\n", byteValue, newValue);
		WriteMemoryValue<uint8_t>(0x005B8EA1 + 1, newValue);
	}
	if (ReadMemoryValue<uint8_t>(0x005C4B42 + 4, byteValue))
	{
		uint8_t newValue = ini.GetLongValue("Guild", "UNION_CHAT_PARTICIPANTS", 12);
		printf(" - GUILD_UNION_CHAT_PARTICIPANTS (%d) -> (%d)\r\n", byteValue, newValue);
		WriteMemoryValue<uint8_t>(0x005C4B42 + 4, newValue);
	}

	// Academy
	if (ReadMemoryValue<uint32_t>(0x005DD36A + 1, uintValue))
	{
		uint32_t newValue = ini.GetLongValue("Academy", "DISBAND_PENALTY_TIME", 604800);
		printf(" - ACADEMY_DISBAND_PENALTY_TIME (%u) -> (%u)\r\n", uintValue, newValue);
		WriteMemoryValue<uint32_t>(0x005DD36A + 1, newValue);
		WriteMemoryValue<uint32_t>(0x00651C7A + 2, newValue);
	}

	// Alchemy
	if (ReadMemoryValue<uint8_t>(0x0052ADAA + 6, byteValue))
	{
		uint8_t newValue = ini.GetLongValue("Alchemy", "FUSING_DELAY", 3);
		printf(" - ALCHEMY_FUSING_DELAY (%d) -> (%d)\r\n", byteValue, newValue);
		WriteMemoryValue<uint8_t>(0x0052ADAA + 6, newValue);
	}
	if (ReadMemoryValue<uint8_t>(0x00506D92 + 2, byteValue))
	{
		uint8_t newValue = ini.GetLongValue("Alchemy", "STONE_ASTRAL_VALUE", 4);
		printf(" - ALCHEMY_STONE_ASTRAL_VALUE (%d) -> (%d)\r\n", byteValue, newValue);
		WriteMemoryValue<uint8_t>(0x00506D92 + 2, newValue);
		WriteMemoryValue<uint8_t>(0x00506DD2 + 1, newValue);
	}

	// Event
	{
		auto currentValue = ReadMemoryString(0x00646D42 + 1);
		if (!currentValue.empty())
		{
			m_CTF_ITEM_WINNING_REWARD = ini.GetValue("Event", "CTF_ITEM_WIN_REWARD", "ITEM_ETC_E080723_ICETROPHY");
			auto newValueLen = m_CTF_ITEM_WINNING_REWARD.size();
			// Check value it's not empty and shorter than 128 bytes
			if (newValueLen != 0 && newValueLen <= 128)
			{
				printf(" - EVENT_CTF_ITEM_WIN_REWARD (%s) -> (%s)\r\n", currentValue.c_str(), m_CTF_ITEM_WINNING_REWARD.c_str());
				// Set char* pointer to the new value
				WriteMemoryValue<uint32_t>(0x00646D42 + 1, (uint32_t)m_CTF_ITEM_WINNING_REWARD.c_str()); // Winning Reward
				WriteMemoryValue<uint32_t>(0x00876935 + 6, (uint32_t)m_CTF_ITEM_WINNING_REWARD.c_str()); // Just in case, something about Quest reward required probably
			}
		}
	}
	if (ReadMemoryValue<uint8_t>(0x00646D40 + 1, byteValue))
	{
		uint8_t newValue = ini.GetLongValue("Event", "CTF_ITEM_WIN_REWARD_AMOUNT", 1);
		printf(" - EVENT_CTF_ITEM_WIN_REWARD_AMOUNT (%d) -> (%d)\r\n", byteValue, newValue);
		WriteMemoryValue<uint8_t>(0x00646D40 + 1, newValue);
	}
	{
		auto currentValue = ReadMemoryString(0x005F19A9 + 1);
		if (!currentValue.empty())
		{
			m_CTF_ITEM_KILLING_REWARD = ini.GetValue("Event", "CTF_ITEM_KILL_REWARD", "ITEM_ETC_E080723_ICETROPHY");
			auto newValueLen = m_CTF_ITEM_KILLING_REWARD.size();
			// Check value it's not empty and shorter than 128 bytes
			if (newValueLen != 0 && newValueLen <= 128)
			{
				printf(" - EVENT_CTF_ITEM_KILL_REWARD (%s) -> (%s)\r\n", currentValue.c_str(), m_CTF_ITEM_KILLING_REWARD.c_str());
				// Set char* pointer to the new value
				WriteMemoryValue<uint32_t>(0x005F19A9 + 1, (uint32_t)m_CTF_ITEM_KILLING_REWARD.c_str()); // Killing Reward
			}
		}
	}
	if (ReadMemoryValue<uint8_t>(0x005F1997 + 1, byteValue))
	{
		uint8_t newValue = ini.GetLongValue("Event", "CTF_ITEM_KILL_REWARD_AMOUNT", 1);
		printf(" - EVENT_CTF_ITEM_KILL_REWARD_AMOUNT (%d) -> (%d)\r\n", byteValue, newValue);
		WriteMemoryValue<uint8_t>(0x005F1997 + 1, newValue);
	}
	{
		auto currentValue = ReadMemoryString(0x006691C6 + 1);
		if (!currentValue.empty())
		{
			m_BA_ITEM_REWARD = ini.GetValue("Event", "BA_ITEM_REWARD", "ITEM_ETC_ARENA_COIN");
			auto newValueLen = m_BA_ITEM_REWARD.size();
			// Check value it's not empty and shorter than 128 bytes
			if (newValueLen != 0 && newValueLen <= 128)
			{
				printf(" - EVENT_BA_ITEM_REWARD (%s) -> (%s)\r\n", currentValue.c_str(), m_BA_ITEM_REWARD.c_str());
				// Set char* pointer to the new value
				WriteMemoryValue<uint32_t>(0x006691C6 + 1, (uint32_t)m_BA_ITEM_REWARD.c_str());
			}
		}
	}
	if (ReadMemoryValue<uint8_t>(0x00669158 + 4, byteValue))
	{
		uint8_t newValue = ini.GetLongValue("Event", "BA_ITEM_REWARD_GJ_W_AMOUNT", 7);
		printf(" - EVENT_BA_ITEM_REWARD_GJ_W_AMOUNT (%d) -> (%d)\r\n", byteValue, newValue);
		WriteMemoryValue<uint8_t>(0x00669158 + 4, newValue);
	}
	if (ReadMemoryValue<uint8_t>(0x00669173 + 4, byteValue))
	{
		uint8_t newValue = ini.GetLongValue("Event", "BA_ITEM_REWARD_GJ_L_AMOUNT", 2);
		printf(" - EVENT_BA_ITEM_REWARD_GJ_L_AMOUNT (%d) -> (%d)\r\n", byteValue, newValue);
		WriteMemoryValue<uint8_t>(0x00669173 + 4, newValue);
	}
	if (ReadMemoryValue<uint8_t>(0x0066915F + 4, byteValue))
	{
		uint8_t newValue = ini.GetLongValue("Event", "BA_ITEM_REWARD_PR_W_AMOUNT", 5);
		printf(" - EVENT_BA_ITEM_REWARD_PR_W_AMOUNT (%d) -> (%d)\r\n", byteValue, newValue);
		WriteMemoryValue<uint8_t>(0x0066915F + 4, newValue);
	}
	if (ReadMemoryValue<uint8_t>(0x0066917A + 4, byteValue))
	{
		uint8_t newValue = ini.GetLongValue("Event", "BA_ITEM_REWARD_PR_L_AMOUNT", 1);
		printf(" - EVENT_BA_ITEM_REWARD_PR_L_AMOUNT (%d) -> (%d)\r\n", byteValue, newValue);
		WriteMemoryValue<uint8_t>(0x0066917A + 4, newValue);
	}

	// Fix
	if (ReadMemoryValue<uint32_t>(0x004744BC + 1, uintValue))
	{
		uint32_t newValue = ini.GetLongValue("Fix", "AGENT_SERVER_CAPACITY", 1000);
		printf(" - FIX_AGENT_SERVER_CAPACITY (%u) -> (%u)\r\n", uintValue, newValue);
		WriteMemoryValue<uint32_t>(0x004744BC + 1, newValue);
		WriteMemoryValue<uint32_t>(0x004744C7 + 1, newValue);
	}
	if (ini.GetBoolValue("Fix", "HIGH_RATES_CONFIG", true))
	{
		printf(" - FIX_HIGH_RATES_CONFIG\r\n");
		WriteMemoryValue<uint8_t>(0x0042714C + 2, 0x42); // ExpRatio
		WriteMemoryValue<uint8_t>(0x004271F5 + 2, 0x42); // ExpRatioParty
		WriteMemoryValue<uint8_t>(0x004272A0 + 2, 0x42); // DropItemRatio
		WriteMemoryValue<uint8_t>(0x00427349 + 2, 0x42); // DropGoldAmountCoef
	}
	if (ini.GetBoolValue("Fix", "DISABLE_GREEN_BOOK", true))
	{
		printf(" - FIX_DISABLE_GREEN_BOOK\r\n");
		for(int i = 0; i < 8; i++)
			WriteMemoryValue<uint8_t>(0x004142E2 + i, 0x90); // NOP
		for(int i = 0; i < 5; i++)
			WriteMemoryValue<uint8_t>(0x0041474D + i, 0x90); // NOP
	}
	if (ini.GetBoolValue("Fix", "DISABLE_MSGBOX_SILK_GOLD_PRICE", true))
	{
		printf(" - FIX_DISABLE_MSGBOX_SILK_GOLD_PRICE\r\n");
		WriteMemoryValue<uint8_t>(0x006A989E, 0xEB); // jne to jmp
		WriteMemoryValue<uint8_t>(0x006A98CB, 0xEB); // jne to jmp
	}
	if (ini.GetBoolValue("Fix", "EXCHANGE_ATTACK_CANCEL", true))
	{
		printf(" - FIX_EXCHANGE_ATTACK_CANCEL\r\n");
		for(int i = 0; i < 2; i++)
			WriteMemoryValue<uint8_t>(0x00515578 + i, 0x90); // NOP call
	}
	if (ini.GetBoolValue("Fix", "EXPLOIT_INVISIBLE_INVINCIBLE", true))
	{
		printf(" - FIX_EXPLOIT_INVISIBLE_INVINCIBLE\r\n");
		for (int i = 0; i < 2; i++)
			WriteMemoryValue<uint8_t>(0x00515B78 + i, 0x90); // NOP jnz
	}
}
void AppManager::InitDatabaseFetch()
{
	std::cout << " * Initializing database connection to execute actions..." << std::endl;

	// Load file
	CSimpleIniA ini;
	ini.LoadFile("vSRO-GameServer.ini");

	// Create connection string
	std::wstringstream connString;
	connString << "DRIVER={SQL Server};";
	connString << "SERVER=" << ini.GetValue("Sql", "HOST", "localhost") << ", " << ini.GetValue("Sql", "PORT", "1433") << ";";
	connString << "DATABASE=" << ini.GetValue("Sql", "DB_SHARD", "SRO_VT_SHARD") << ";";
	connString << "UID=" << ini.GetValue("Sql", "USER", "sa") << ";";
	connString << "PWD=" << ini.GetValue("Sql", "PASS", "1234") << ";";

	if (m_dbLink.sqlConn.Open((SQLWCHAR*)connString.str().c_str()) && m_dbLink.sqlCmd.Open(m_dbLink.sqlConn)
		&& m_dbLinkHelper.sqlConn.Open((SQLWCHAR*)connString.str().c_str()) && m_dbLinkHelper.sqlCmd.Open(m_dbLinkHelper.sqlConn))
	{
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)AppManager::DatabaseFetchThread, 0, 0, 0);
	}
}
DWORD WINAPI AppManager::DatabaseFetchThread()
{
	// Load file
	CSimpleIniA ini;
	ini.LoadFile("vSRO-GameServer.ini");
	const char* fetchTableName = ini.GetValue("Sql", "DB_SHARD_FETCH_TABLE", "_ExeGameServer");

	// Generate unique id to fetch from multiples instances
	int pId = GetProcessInstanceId();
	std::string suffix = (pId == 0 ? "" : std::to_string(pId + 1));
	const char* fetchTableSuffix = suffix.c_str();
	
	// Show a message about table to be fetch
	std::cout << " - Waiting 1min before start fetching on \"" << fetchTableName << fetchTableSuffix << "\"..." << std::endl;
	Sleep(60000);
	std::cout << " - Fetching started!" << std::endl;
	m_IsRunningDatabaseFetch = true;

	// Try to create table used to fetch
	std::wstringstream qCreateTable;
	qCreateTable << "IF OBJECT_ID(N'dbo." << fetchTableName << fetchTableSuffix << "', N'U') IS NULL";
	qCreateTable << " BEGIN";
	qCreateTable << " CREATE TABLE dbo." << fetchTableName << fetchTableSuffix;
	qCreateTable << " (ID INT IDENTITY(1,1) PRIMARY KEY,";
	qCreateTable << " Action_ID INT NOT NULL,";
	qCreateTable << " Action_Result SMALLINT NOT NULL DEFAULT 0,";
	qCreateTable << " CharName16 VARCHAR(64) NOT NULL,";
	qCreateTable << " Param01 VARCHAR(129),";
	qCreateTable << " Param02 BIGINT,";
	qCreateTable << " Param03 BIGINT,";
	qCreateTable << " Param04 BIGINT,";
	qCreateTable << " Param05 BIGINT,";
	qCreateTable << " Param06 BIGINT,";
	qCreateTable << " Param07 BIGINT,";
	qCreateTable << " Param08 BIGINT)";
	qCreateTable << " END";

	// Try execute query
	if (!m_dbLink.sqlCmd.ExecuteQuery((SQLWCHAR*)qCreateTable.str().c_str())) {
		m_IsRunningDatabaseFetch = false;
		return 0;
	}
	m_dbLink.sqlCmd.Clear();

	// Stops this thread loop on interruption/exit
	signal(SIGINT, [](int) {
		m_IsRunningDatabaseFetch = false;
	});

	// Start fetching actions without result
	std::wstringstream qSelectActions;
	qSelectActions << "SELECT ID, Action_ID, CharName16, Param01, Param02, Param03, Param04, Param05, Param06, Param07, Param08";
	qSelectActions << " FROM dbo." << fetchTableName << fetchTableSuffix;
	qSelectActions << " WHERE Action_Result = " << FETCH_ACTION_STATE::UNKNOWN;
	while (m_IsRunningDatabaseFetch)
	{
		// Try to execute query
		if (!m_dbLink.sqlCmd.ExecuteQuery((SQLWCHAR*)qSelectActions.str().c_str()))
			break;

		// Fetch one by one
		while (m_dbLink.sqlCmd.FetchData())
		{
			// Set default state
			FETCH_ACTION_STATE actionResult = FETCH_ACTION_STATE::SUCCESS;
			
			// Read required params
			SQLINTEGER cID, cActionID;
			char cCharName[64];
			m_dbLink.sqlCmd.GetData(1, SQL_C_ULONG, &cID, 0, NULL);
			m_dbLink.sqlCmd.GetData(2, SQL_C_ULONG, &cActionID, 0, NULL);
			m_dbLink.sqlCmd.GetData(3, SQL_C_CHAR, &cCharName, 64, 0);

			// Try to execute the action
			try {
				switch (cActionID)
				{
				case 1: // Add Item
				{
					// Read & check params
					char cParam01[128];
					SQLUINTEGER cParam02;
					SQLINTEGER cParam03;
					SQLUSMALLINT cParam04;
					if (m_dbLink.sqlCmd.GetData(4, SQL_C_CHAR, &cParam01, 128, 0)
						&& m_dbLink.sqlCmd.GetData(5, SQL_C_ULONG, &cParam02, 0, NULL)
						&& m_dbLink.sqlCmd.GetData(6, SQL_C_ULONG, &cParam03, 0, NULL)
						&& m_dbLink.sqlCmd.GetData(7, SQL_C_ULONG, &cParam04, 0, NULL))
					{
						// Check player existence
						CGObjPC* player = CGObjManager::GetObjPCByCharName16(cCharName);
						if (player)
						{
							auto operationCode = player->AddItem(cParam01, cParam02, cParam03, cParam04);
							if(operationCode != 1)
							{
								std::cout << " > Unnexpected AddItem on [" << cCharName << "] Result [" << operationCode << "]" << std::endl;
								actionResult = FETCH_ACTION_STATE::FUNCTION_ERROR;
							}						
						}
						else
							actionResult = FETCH_ACTION_STATE::CHARNAME_NOT_FOUND;
					}
					else
						actionResult = FETCH_ACTION_STATE::PARAMS_NOT_SUPPLIED;
				} break;
				case 2: // Update Gold
				{
					// Read params
					SQLBIGINT cParam02;
					if (m_dbLink.sqlCmd.GetData(5, SQL_C_SBIGINT, &cParam02, 0, NULL))
					{
						// Check player existence
						CGObjPC* player = CGObjManager::GetObjPCByCharName16(cCharName);
						if (player)
							player->UpdateGold(cParam02);
						else
							actionResult = FETCH_ACTION_STATE::CHARNAME_NOT_FOUND;
					}
					else
						actionResult = FETCH_ACTION_STATE::PARAMS_NOT_SUPPLIED;
				} break;
				case 3: // Update Hwan level (Berserk title)
				{
					// Read params
					SQLUSMALLINT cParam02;
					if (m_dbLink.sqlCmd.GetData(5, SQL_C_USHORT, &cParam02, 0, NULL))
					{
						// Check player existence
						CGObjPC* player = CGObjManager::GetObjPCByCharName16(cCharName);
						if (player)
							player->UpdateHwan(cParam02);
						else
							actionResult = FETCH_ACTION_STATE::CHARNAME_NOT_FOUND;
					}
					else
						actionResult = FETCH_ACTION_STATE::PARAMS_NOT_SUPPLIED;
				} break;
				case 4: // Move Player
				{
					// Read params
					SQLUSMALLINT cParam02, cParam03, cParam04;
					SQLSMALLINT cParam05;
					if (m_dbLink.sqlCmd.GetData(5, SQL_C_USHORT, &cParam02, 0, NULL)
						&& m_dbLink.sqlCmd.GetData(6, SQL_C_USHORT, &cParam03, 0, NULL)
						&& m_dbLink.sqlCmd.GetData(7, SQL_C_SHORT, &cParam04, 0, NULL)
						&& m_dbLink.sqlCmd.GetData(8, SQL_C_USHORT, &cParam05, 0, NULL))
					{
						// Check player existence
						CGObjPC* player = CGObjManager::GetObjPCByCharName16(cCharName);
						if (player)
						{
							if(!player->MoveTo(cParam02, cParam03, cParam04, cParam05))
								actionResult = FETCH_ACTION_STATE::FUNCTION_ERROR;
						}
						else
							actionResult = FETCH_ACTION_STATE::CHARNAME_NOT_FOUND;
					}
					else
						actionResult = FETCH_ACTION_STATE::PARAMS_NOT_SUPPLIED;
				} break;
				case 5: // Move Player through WorldId
				{
					// Read & check params
					SQLUSMALLINT cParam02, cParam03, cParam04, cParam06;
					SQLSMALLINT cParam05;
					if ( m_dbLink.sqlCmd.GetData(5, SQL_C_USHORT, &cParam02, 0, NULL)
						&& m_dbLink.sqlCmd.GetData(6, SQL_C_USHORT, &cParam03, 0, NULL)
						&& m_dbLink.sqlCmd.GetData(7, SQL_C_USHORT, &cParam04, 0, NULL)
						&& m_dbLink.sqlCmd.GetData(8, SQL_C_SHORT, &cParam05, 0, NULL)
						&& m_dbLink.sqlCmd.GetData(9, SQL_C_USHORT, &cParam06, 0, NULL))
					{
						// Check player existence
						CGObjPC* player = CGObjManager::GetObjPCByCharName16(cCharName);
						if (player)
						{
							if(!player->MoveTo(cParam02 + 0x10000, cParam03, cParam04, cParam05, cParam06))
								actionResult = FETCH_ACTION_STATE::FUNCTION_ERROR;
						}
						else
							actionResult = FETCH_ACTION_STATE::CHARNAME_NOT_FOUND;
					}
					else
						actionResult = FETCH_ACTION_STATE::PARAMS_NOT_SUPPLIED;				
				} break;
				case 6: // Drop item near player
				{
					// Read & check params
					char cParam01[128];
					SQLUINTEGER cParam02;
					SQLUSMALLINT cParam03;
					if (m_dbLink.sqlCmd.GetData(4, SQL_C_CHAR, &cParam01, 128, 0)
						&& m_dbLink.sqlCmd.GetData(5, SQL_C_ULONG, &cParam02, 0, NULL)
						&& m_dbLink.sqlCmd.GetData(6, SQL_C_USHORT, &cParam03, 0, NULL))
					{
						// Check player existence
						CGObjPC* player = CGObjManager::GetObjPCByCharName16(cCharName);
						if (player)
							player->DropItem(cParam01, cParam02, cParam03);
						else
							actionResult = FETCH_ACTION_STATE::CHARNAME_NOT_FOUND;
					}
					else
						actionResult = FETCH_ACTION_STATE::PARAMS_NOT_SUPPLIED;
				} break;
				case 7: // Transform item from inventory slot
				{ 
					// Read & check params
					char cParam01[128];
					SQLUSMALLINT cParam02;
					if (m_dbLink.sqlCmd.GetData(4, SQL_C_CHAR, &cParam01, 128, 0)
						&& m_dbLink.sqlCmd.GetData(5, SQL_C_USHORT, &cParam02, 0, NULL))
					{
						// Check player existence
						CGObjPC* player = CGObjManager::GetObjPCByCharName16(cCharName);
						if (player)
						{
							if (!player->MutateItemAt(cParam02, cParam01))
								actionResult = FETCH_ACTION_STATE::FUNCTION_ERROR;
						}
						else
							actionResult = FETCH_ACTION_STATE::CHARNAME_NOT_FOUND;
					}
					else
						actionResult = FETCH_ACTION_STATE::PARAMS_NOT_SUPPLIED;
				} break;
				case 8: // Force reloading player
				{ 
					// Check player existence
					CGObjPC* player = CGObjManager::GetObjPCByCharName16(cCharName);
					if (player)
					{
						if(!player->Reload())
							actionResult = FETCH_ACTION_STATE::FUNCTION_ERROR;
					}
					else
						actionResult = FETCH_ACTION_STATE::CHARNAME_NOT_FOUND;
				} break;
				case 9: // Add buff to player
				{
					// Read & check params
					SQLUINTEGER cParam02, cParam03;
					if (m_dbLink.sqlCmd.GetData(5, SQL_C_ULONG, &cParam02, 0, NULL)
						&& m_dbLink.sqlCmd.GetData(6, SQL_C_ULONG, &cParam03, 0, NULL))
					{
						// Check player existence
						CGObjPC* player = CGObjManager::GetObjPCByCharName16(cCharName);
						if (player)
							player->AddBuff(cParam02, cParam03);
						else
							actionResult = FETCH_ACTION_STATE::CHARNAME_NOT_FOUND;
					}
					else
						actionResult = FETCH_ACTION_STATE::PARAMS_NOT_SUPPLIED;
				} break;
				case 10: // Create mob in map location
				{
					// Read & check params
					SQLUINTEGER cParam02;
					SQLUSMALLINT cParam03, cParam04, cParam06;
					SQLSMALLINT cParam05;
					if (m_dbLink.sqlCmd.GetData(5, SQL_C_ULONG, &cParam02, 0, NULL)
						&& m_dbLink.sqlCmd.GetData(6, SQL_C_USHORT, &cParam03, 0, NULL)
						&& m_dbLink.sqlCmd.GetData(7, SQL_C_USHORT, &cParam04, 0, NULL)
						&& m_dbLink.sqlCmd.GetData(8, SQL_C_SHORT, &cParam05, 0, NULL)
						&& m_dbLink.sqlCmd.GetData(9, SQL_C_USHORT, &cParam06, 0, NULL))
					{
						// Try to create mob
						if (!CGObjManager::CreateMob(cParam02, 0x10001, cParam03, cParam04, cParam05, cParam06, 10))
							actionResult = FETCH_ACTION_STATE::FUNCTION_ERROR;
					}
					else
						actionResult = FETCH_ACTION_STATE::PARAMS_NOT_SUPPLIED;
				} break;
				case 11: // Create mob in map location through world id
				{
					// Read & check params
					SQLUINTEGER cParam02;
					SQLUSMALLINT cParam03, cParam04, cParam05, cParam07;
					SQLSMALLINT cParam06;
					if (m_dbLink.sqlCmd.GetData(5, SQL_C_ULONG, &cParam02, 0, NULL)
						&& m_dbLink.sqlCmd.GetData(6, SQL_C_USHORT, &cParam03, 0, NULL)
						&& m_dbLink.sqlCmd.GetData(7, SQL_C_USHORT, &cParam04, 0, NULL)
						&& m_dbLink.sqlCmd.GetData(8, SQL_C_USHORT, &cParam05, 0, NULL)
						&& m_dbLink.sqlCmd.GetData(9, SQL_C_SHORT, &cParam06, 0, NULL)
						&& m_dbLink.sqlCmd.GetData(10, SQL_C_USHORT, &cParam07, 0, NULL))
					{
						// Try to create mob
						if (!CGObjManager::CreateMob(cParam02, cParam03 + 0x10000, cParam04, cParam05, cParam06, cParam07, 10))
							actionResult = FETCH_ACTION_STATE::FUNCTION_ERROR;
					}
					else
						actionResult = FETCH_ACTION_STATE::PARAMS_NOT_SUPPLIED;
				} break;
				case 12: // Set body mode from player
				{
					SQLUSMALLINT cParam02;
					if (m_dbLink.sqlCmd.GetData(5, SQL_C_USHORT, &cParam02, 0, NULL))
					{
						CGObjPC* player = CGObjManager::GetObjPCByCharName16(cCharName);
						if (player)
							player->SetBodyMode(cParam02);
						else
							actionResult = FETCH_ACTION_STATE::CHARNAME_NOT_FOUND;
					}
				} break;
				case 13: // Update Skill Points 
				{
					SQLINTEGER cParam02;
					if (m_dbLink.sqlCmd.GetData(5, SQL_C_LONG, &cParam02, 0, NULL))
					{
						CGObjPC* player = CGObjManager::GetObjPCByCharName16(cCharName);
						if (player)
							player->UpdateSP(cParam02);
						else
							actionResult = FETCH_ACTION_STATE::CHARNAME_NOT_FOUND;
					}
				} break;
				case 14: // Change Guild NickName
				{
					char cParam01[13];
					if (m_dbLink.sqlCmd.GetData(4, SQL_C_CHAR, &cParam01, 13, 0))
					{
						CGObjPC* player = CGObjManager::GetObjPCByCharName16(cCharName);
						if (player)
							player->ApplyGuildNickName(cParam01);
						else
							actionResult = FETCH_ACTION_STATE::CHARNAME_NOT_FOUND;
					}
				} break;
				case 15: // Set Life State
				{
					SQLUSMALLINT cParam02;
					if (m_dbLink.sqlCmd.GetData(5, SQL_C_USHORT, &cParam02, 0, NULL))
					{
						CGObjPC* player = CGObjManager::GetObjPCByCharName16(cCharName);
						if (player)
							player->SetLifeState(cParam02 != 0);
						else
							actionResult = FETCH_ACTION_STATE::CHARNAME_NOT_FOUND;
					}
				} break;
				case 16: // Update Experience
				{
					SQLBIGINT cParam02;
					if (m_dbLink.sqlCmd.GetData(5, SQL_C_SBIGINT, &cParam02, 0, NULL))
					{
						CGObjPC* player = CGObjManager::GetObjPCByCharName16(cCharName);
						if (player)
							player->UpdateExperience(cParam02);
						else
							actionResult = FETCH_ACTION_STATE::CHARNAME_NOT_FOUND;
					}
				} break;
				case 17: // Add Skill Point Experience
				{
					SQLUINTEGER cParam02;
					if (m_dbLink.sqlCmd.GetData(5, SQL_C_ULONG, &cParam02, 0, NULL))
					{
						CGObjPC* player = CGObjManager::GetObjPCByCharName16(cCharName);
						if (player)
							player->AddSPExperience(cParam02);
						else
							actionResult = FETCH_ACTION_STATE::CHARNAME_NOT_FOUND;
					}
				} break;
				case 18: // Update PVP cape type
				{
					SQLUSMALLINT cParam02;
					if (m_dbLink.sqlCmd.GetData(5, SQL_C_USHORT, &cParam02, 0, NULL))
					{
						CGObjPC* player = CGObjManager::GetObjPCByCharName16(cCharName);
						if (player)
							player->UpdatePVPCapeType(cParam02);
						else
							actionResult = FETCH_ACTION_STATE::CHARNAME_NOT_FOUND;
					}
				} break;
				case 19: // Reduce HP/MP from player
				{
					SQLINTEGER cParam02, cParam03;
					if (m_dbLink.sqlCmd.GetData(5, SQL_C_LONG, &cParam02, 0, NULL)
						&& m_dbLink.sqlCmd.GetData(6, SQL_C_LONG, &cParam03, 0, NULL))
					{
						CGObjPC* player = CGObjManager::GetObjPCByCharName16(cCharName);
						if (player)
							player->ReduceHPMP(cParam02, cParam03, true);
						else
							actionResult = FETCH_ACTION_STATE::CHARNAME_NOT_FOUND;
					}
				} break;
				case 3312: // For testing references
				{
					CGObjPC* player = CGObjManager::GetObjPCByCharName16(cCharName);
					if (player)
					{
						std::cout << " - CGObjPC ptr: " << player << "\r\n Unique Id: " << player->GetUniqueId() << "\r\n";
					}
					else
						actionResult = FETCH_ACTION_STATE::CHARNAME_NOT_FOUND;
				} break;
				default:
					std::cout << " Error on Action_ID (" << cActionID << ") : Undefined" << std::endl;
					actionResult = FETCH_ACTION_STATE::ACTION_UNDEFINED;
					break;
				}
			}
			catch (std::exception& ex)
			{
				std::cout << " Exception on Action_ID (" << cActionID << ") : " << ex.what() << std::endl;
				actionResult = FETCH_ACTION_STATE::UNNEXPECTED_EXCEPTION;
			}

			// Update action result from table by row id
			std::wstringstream qUpdateResult;
			qUpdateResult << "UPDATE dbo." << fetchTableName << fetchTableSuffix;
			qUpdateResult << " SET Action_Result = " << actionResult;
			qUpdateResult << " WHERE ID = " << cID;
			m_dbLinkHelper.sqlCmd.ExecuteQuery((SQLWCHAR*)qUpdateResult.str().c_str());
			m_dbLinkHelper.sqlCmd.Clear();
		}
		m_dbLink.sqlCmd.Clear();

		// Making like 10 querys per second
		Sleep(100);
	}

	// Close connection and dispose handlers
	m_dbLinkHelper.sqlConn.Close();
	m_dbLink.sqlConn.Close();

	// Stop flag
	m_IsRunningDatabaseFetch = false;
	std::cout << " - Fetching stopped!" << std::endl;

	return 0;
}

int AppManager::GetProcessInstanceId()
{
    // Check unique process instances using the executable path
    std::string path = GetExecutablePath();
    StringReplaceAll(path, "\\", "/"); // Replace special symbols used on mutex

    // Find available id
    int id = 0;
    while (true)
    {
        // Set an unique name as ID
        std::stringstream ss;
        ss << "Global\\" << path.c_str() << "|" << id;

        // Try to create mutex
        CreateMutexA(NULL, TRUE, ss.str().c_str());
        if (GetLastError() != ERROR_ALREADY_EXISTS)
            break;
        // Try to find another id
        id++;
    }
    return id;
}

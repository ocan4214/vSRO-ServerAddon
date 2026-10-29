#pragma once
#include <Windows.h>
#include "Database/SQLConnection.h"
#include "Database/SQLCommand.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <mutex>

struct PlayerStatProgress {
	uint32_t currentKills;
	uint32_t bonusGranted;
};

// All states fetching can generate
enum FETCH_ACTION_STATE {
	UNKNOWN = 0,
	SUCCESS = 1,
	ACTION_UNDEFINED = 2,
	UNNEXPECTED_EXCEPTION = 3,
	PARAMS_NOT_SUPPLIED = 4,
	CHARNAME_NOT_FOUND = 5,
	FUNCTION_ERROR = 6
};

// Handlers required to make a database link
struct DatabaseLink {
	SQLConnection sqlConn;
	SQLCommand sqlCmd;
};

// Application Manager sharing info to any place in the project
class AppManager
{
private: // Private members
	// Check if app has been initialized
	static bool m_IsInitialized;
	// Handlers for SQL communication
	static DatabaseLink m_dbLink, m_dbLinkHelper, m_dbUniqueLog;
	// Flag to keep thread safe
	static bool m_IsRunningDatabaseFetch;
	// Keeps in memory the value assigned
	static std::string m_CTF_ITEM_WINNING_REWARD;
	static std::string m_CTF_ITEM_KILLING_REWARD;
	static std::string m_BA_ITEM_REWARD;
	static std::string m_SPOOF_IP;
public: // Public Methods
	// Initialize manager
	static void Initialize();
	// Initialize the default config file if doesn't exists
	static void InitConfigFile();
	// Mob Kill Stats Config
	static bool m_MobKillStats_Enabled;
	static uint32_t m_MobKillStats_RequiredCount;
	static std::string m_MobKillStats_RewardType;
	static uint16_t m_MobKillStats_RewardAmount;
	static bool m_MobKillStats_LevelDiffCheck;
	static uint32_t m_MobKillStats_MaxRewardCap;
	static bool m_MobKillStats_VerboseLog;
	// Global crash filter creating MiniDump file
	static LONG WINAPI CrashDumpFilter(EXCEPTION_POINTERS* pExceptionPointers);
	// Thread-safe player kill tracker map
	static std::unordered_map<uint32_t, PlayerStatProgress> m_MobKillTracker;
	static std::mutex m_MobKillMutex;

	// Combat kill event hooks
	static void __stdcall OnEntityKilled(void* pAttacker, void* pDeadEntity);
	static void __stdcall OnEntityKilledParty(void* pAttacker, void* pDeadEntity);
	static void __stdcall OnUniversalKillRewardBridge(void* pAttacker, void* pVictim);
	static void ProcessMobKillReward(void* pAttacker, void* pDeadEntity);
	// Thread-safe logger writing to console and vSRO-MobKillStats.log
	static void LogMobKill(const char* format, ...);
private: // Private Helpers
	// Internal worker for mob kill reward logic (called under SEH guard)
	static void ProcessMobKillRewardInternal(void* pAttacker, void* pDeadEntity);
	// Starts console if required
	static void InitDebugConsole();
	// Set all offsets values
	static void InitPatchValues();
	// Initialize Mob Kill Stats configuration
	static void InitMobKillStats();
	// Initialize all hooks required
	static void InitHooks();
	// Event handler for unique spawn
	static void OnUniqueSpawnMsg(uint32_t LogType, const char* Message, const char* UniqueCodeName, uint16_t RegionId, uint32_t unk01, uint32_t unk02);
	// Event handler for players killing uniques
	static void OnUniqueKilledMsg(uint32_t LogType, const char* Message, const char* UniqueCodeName, const char* CharName);
	// Starts to fetch database info
	static void InitDatabaseFetch();
	// Fetch database and execute the required actions
	static DWORD WINAPI DatabaseFetchThread();
	// Creates an unique identifier for this process starting from zero
	static int GetProcessInstanceId();
};
// Debugger.cpp : vSRO-GameServer.ini Olusturucu & Test Araci
//

#include <iostream>
#include "../vSRO-GameServer/Utils/IO/SimpleIni.h"

int main()
{
    std::cout << "[Debugger] vSRO-GameServer.ini olusturuluyor..." << std::endl;

    CSimpleIniA ini;
    ini.SetSpaces(false);

    // Sql
    ini.SetValue("Sql", "HOST", "localhost", "; SQL Server address or name");
    ini.SetValue("Sql", "PORT", "1433", "; SQL Server port");
    ini.SetValue("Sql", "USER", "sa", "; Username credential");
    ini.SetValue("Sql", "PASS", "1234", "; Password credential");
    ini.SetValue("Sql", "DB_SHARD", "SRO_VT_SHARD", "; Name used for the specified silkroad database");
    ini.SetValue("Sql", "DB_SHARD_FETCH_TABLE", "_ExeGameServer", "; Table name used to execute actions");
    ini.SetValue("Sql", "DB_LOG", "SRO_VT_SHARDLOG", "; Name used for the specified silkroad database");

    // Server
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

    // Job
    ini.SetLongValue("Job", "LEVEL_MAX", 7, "; Maximum level that can be reached on job suit");
    ini.SetBoolValue("Job", "DISABLE_MOB_SPAWN", false, "; Disable Thief/Hunter monster spawn while trading");
    ini.SetLongValue("Job", "TEMPLE_LEVEL", 105, "; Minimum level to enter the Temple Area");

    // Race
    ini.SetLongValue("Race", "CH_TOTAL_MASTERIES", 330, "; Masteries amount Chinese will obtain");
    ini.SetLongValue("Race", "EU_TOTAL_MASTERIES", 220, "; Masteries amount European will obtain");

    // Guild
    ini.SetLongValue("Guild", "MEMBERS_LIMIT_LEVEL1", 15, "; Guild members capacity at level 1");
    ini.SetLongValue("Guild", "MEMBERS_LIMIT_LEVEL2", 20, "; Guild members capacity at level 2");
    ini.SetLongValue("Guild", "MEMBERS_LIMIT_LEVEL3", 25, "; Guild members capacity at level 3");
    ini.SetLongValue("Guild", "MEMBERS_LIMIT_LEVEL4", 35, "; Guild members capacity at level 4");
    ini.SetLongValue("Guild", "MEMBERS_LIMIT_LEVEL5", 50, "; Guild members capacity at level 5");
    ini.SetLongValue("Guild", "STORAGE_SLOTS_MIN", 30, "; Storage slots at first level");
    ini.SetLongValue("Guild", "STORAGE_SLOTS_INCREASE", 30, "; Storage slots increased per level");
    ini.SetLongValue("Guild", "UNION_LIMIT", 8, "; Union participants limit");
    ini.SetLongValue("Guild", "UNION_CHAT_PARTICIPANTS", 12, "; Union chat participants allowed by guild");

    // Academy
    ini.SetLongValue("Academy", "DISBAND_PENALTY_TIME", 604800, "; Penalty time (seconds) to create again the group");

    // Alchemy
    ini.SetLongValue("Alchemy", "FUSING_DELAY", 3, "; Waiting delay (seconds) after fusing alchemy elements");
    ini.SetLongValue("Alchemy", "STONE_ASTRAL_VALUE", 4, "; Value from astral stones used to stop and prevent +0");

    // Event
    ini.SetValue("Event", "CTF_ITEM_WIN_REWARD", "ITEM_ETC_E080723_ICETROPHY", "; Item reward from winning Capture The Flag");
    ini.SetLongValue("Event", "CTF_ITEM_WIN_REWARD_AMOUNT", 1, "; Amount to obtain by winning");
    ini.SetValue("Event", "CTF_ITEM_KILL_REWARD", "ITEM_ETC_E080723_ICETROPHY", "; Item reward from killing at Capture The Flag");
    ini.SetLongValue("Event", "CTF_ITEM_KILL_REWARD_AMOUNT", 1, "; Amount to obtain by kill");
    ini.SetValue("Event", "BA_ITEM_REWARD", "ITEM_ETC_ARENA_COIN", "; Item reward from Battle Arena");
    ini.SetLongValue("Event", "BA_ITEM_REWARD_GJ_W_AMOUNT", 7, "; Amount to obtain winning on Guild/Job mode");
    ini.SetLongValue("Event", "BA_ITEM_REWARD_GJ_L_AMOUNT", 2, "; Amount to obtain loosing");
    ini.SetLongValue("Event", "BA_ITEM_REWARD_PR_W_AMOUNT", 5, "; Amount to obtain winning on Party/Random mode");
    ini.SetLongValue("Event", "BA_ITEM_REWARD_PR_L_AMOUNT", 1, "; Amount to obtain loosing");

    // Fix
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
    ini.SetLongValue("MobKillStats", "MOBS_PER_REWARD", 5, "; Monsters required to get reward (Hizli test icin 5)");
    ini.SetValue("MobKillStats", "REWARD_TYPE", "STR", "; Reward type: STR, INT, or STATPOINT");
    ini.SetLongValue("MobKillStats", "REWARD_AMOUNT", 1, "; Stat points awarded per milestone");
    ini.SetBoolValue("MobKillStats", "LEVEL_DIFF_CHECK", false, "; Only count mobs within player level range (+- 10)");
    ini.SetLongValue("MobKillStats", "MAX_REWARD_CAP", 50, "; Maximum total bonus stats a player can receive");
    ini.SetBoolValue("MobKillStats", "VERBOSE_LOG", true, "; Output verbose progress log on every kill (1=ON, 0=OFF)");
    ini.SetLongValue("MobKillStats", "MAX_SAFE_STAT_LIMIT", 32000, "; Safety clamp to prevent smallint (32767) DB & client overflow");
    ini.SetBoolValue("MobKillStats", "SEND_CHAT_MESSAGE", true, "; Send in-game chat message upon milestone achievement");
    ini.SetLongValue("MobKillStats", "CHAT_TYPE", 9, "; Chat type (9=PM/Pink Whisper, 2=Whisper, 1=General, 7=Notice)");
    ini.SetValue("MobKillStats", "CHAT_SENDER_NAME", "[Server]", "; Sender name for the message");
    ini.SetValue("MobKillStats", "CHAT_MESSAGE", "Tebrikler! %u canavar kestiniz ve +%u %s kazandiniz! (Bonus: %u/%u)", "; In-game message format template");

    // App
    ini.SetBoolValue("App", "DEBUG_CONSOLE", true, "; Attach debug console");

    // Save
    ini.SaveFile("vSRO-GameServer.ini");

    std::cout << "[Debugger SUCCESS] vSRO-GameServer.ini dosyasi basariyla olusturuldu!" << std::endl;
    return 0;
}

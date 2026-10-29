#pragma once
#include "CGObjChar.h"
#include "../Navigation/CPosition.h"
#include "CInstancePC.h"

// Game object for a player character
class CGObjPC : public CGObjChar
{
private: /// Private Members
	char pad_0x8[44];
	CInstancePC* m_CInstancePC;
	char pad_0x38[76];
	CPosition m_Position;
public: /// Public Properties
	// Gets the CharId from database
	uint32_t GetCharID();
	// Gets the current position
	CPosition GetPosition();
	// Gets the character instance pointer
	CInstancePC* GetInstance() { return m_CInstancePC; }
public: /// Public Methods
	// Dispatches 0x303D stat update packet to client
	void SendStatUpdate();
	// Dispatches 0x304E packet to client (type 1=Gold, 2=SP, 3=RemainStatPoint, 4=Hwan)
	void SendStuffUpdate(uint8_t updateType, bool showMessage = false);
	// Dispatches 0x3026 chat message packet to client (type 9=Incoming Whisper/Pink, 2=PM, 1=General, 7=Notice)
	void SendChatMessage(uint8_t chatType, const char* senderName, const char* message);
	// Dispatches an incoming whisper packet (0x3026, Type 9) to client displayed in pink
	void SendWhisper(const char* senderName, const char* message);
	// Add item to inventory. Return error code
	uint16_t AddItem(const char* Codename, int32_t Count, bool RandomizeStats, int8_t OptLevel);
	// Update the gold this player has
	void UpdateGold(int64_t Offset);
	// Update hwan title by level
	void UpdateHwan(uint8_t Level);
	// Update the current Skill Points
	void UpdateSP(int32_t Offset);
	// Update level experience
	void UpdateExperience(int64_t ExpOffset);
	// Add skill experience
	void AddSPExperience(uint32_t SPExpOffset);
	// Reduces health and/or mana points. If health reduced equals or exceeds the current amount, the player will die
	void ReduceHPMP(uint32_t Health, uint32_t Mana, bool ShowEffect);
	// Updates the cape state from PVP
	void UpdatePVPCapeType(uint8_t CapeType);
	// Moves the player to the map location. Return success
	bool MoveTo(uint16_t RegionId, float PosX, float PosY, float PosZ);
	// Moves the player to the gameworld and map location. Return success
	bool MoveTo(uint32_t GameWorldId, uint16_t RegionId, float PosX, float PosY, float PosZ);
	// Transform an item to another one from inventory slot
	bool MutateItemAt(uint8_t Slot,const char* NewCodename);
	// Moves the player to the same position to force a reloading
	bool Reload();
	// Set the guild member nickname
	void ApplyGuildNickName(const char* Nickname);
private: /// Private Helpers
	// Update the gold amount
	void UpdateGold(int64_t Amount, int32_t Unknown, bool Realtime, bool ShowMessage);
	// Moves the player to the location specified
	bool MoveTo(const uint32_t GameWorldId, CPosition Position, int32_t Type);
};
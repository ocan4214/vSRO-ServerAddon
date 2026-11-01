#include "CGObjPC.h"
#include "../../Utils/Memory/hook.h"
#include "../../Silkroad/Text/GString.h"

/* Public Properties */
uint32_t CGObjPC::GetCharID()
{
	return m_CInstancePC->CharID;
}
CPosition CGObjPC::GetPosition()
{
	return m_Position;
}
/* Public Methods */
void CGObjPC::SendStatUpdate()
{
	reinterpret_cast<void(__thiscall*)(CGObjPC*)>(0x004EBE30)(this);
}
void CGObjPC::SendStuffUpdate(uint8_t updateType, bool showMessage)
{
	uint32_t argType = updateType;
	uint32_t argMsg = showMessage ? 1 : 0;
	void* pThis = this;
	__asm
	{
		push esi
		push edi
		push ebx

		mov edi, pThis
		push argMsg
		push argType

		mov eax, 0x004E3FB0
		call eax

		pop ebx
		pop edi
		pop esi
	}
}
// Helper for Joymax PacketWrite (0x00404090)
// Calling convention: ecx = pPacket, eax = data size, push pData, ret 4
static void PacketWrite(void* packet, const void* data, size_t dataSize)
{
	__asm
	{
		push esi
		push edi
		mov ecx, packet
		mov eax, dataSize
		push data
		mov edx, 0x00404090
		call edx
		pop edi
		pop esi
	}
}

// Chat payload string: 2-byte length + ASCII bytes, like the engine's own 0x3026 producers.
static void PacketWriteString(void* packet, const char* text)
{
	uint16_t len = text ? static_cast<uint16_t>(strlen(text)) : 0;
	PacketWrite(packet, &len, sizeof(uint16_t));
	if (len > 0)
		PacketWrite(packet, text, len);
}

void CGObjPC::SendChatMessage(uint8_t chatType, const char* senderName, const char* message)
{
	if (!message)
		return;

	// 1. Allocate packet for opcode 0x3026 (SERVER_CHAT)
	// vftable[158] is CGObjPC::AllocatePacket(uint16_t opcode) at 0x004E0830
	void* pPacket = CallVirtual<void*(__thiscall*)(CGObjPC*, uint16_t)>(this, 158)(this, 0x3026);
	if (!pPacket)
		return;

	// 2. Chat Type (1 byte)
	PacketWrite(pPacket, &chatType, sizeof(uint8_t));

	// 3. Body. The client's 0x3026 parser (sro_client 0x00877810) reads a DIFFERENT
	//    layout per chat type - it is not "sender + message" for every type:
	//      1 All / 3 AllGM / 13 NPC : [entityId:u32][message]
	//      7 Notice                 : [message]              (also pops the centre banner)
	//      everything else          : [sender][message]
	//    Sending the generic shape with type 3 made the client read the sender length as
	//    an entity id and the next two name bytes as the message length -> the line never
	//    rendered (2026-11-01).
	switch (chatType)
	{
	case 0x01: // CHAT_All
	case 0x03: // CHAT_AllGM
	case 0x0D: // CHAT_NPC
	{
		// The client resolves the sender name from this entity id; 0 => the line is
		// shown as "???". Use types 2/4/5/6/10/11 if a real sender name is wanted.
		uint32_t entityId = 0;
		PacketWrite(pPacket, &entityId, sizeof(uint32_t));
		PacketWriteString(pPacket, message);
		break;
	}
	case 0x07: // CHAT_Notice
		PacketWriteString(pPacket, message);
		break;
	default: // 0x02 PM, 0x04 Party, 0x05 Guild, 0x06 Global, 0x09 Stall, 0x0A Ask, 0x0B Union, 0x10 Academy
		PacketWriteString(pPacket, senderName);
		PacketWriteString(pPacket, message);
		break;
	}

	// 4. Send packet to client
	// vftable[159] is CGObjPC::SendPacket(void* pPacket) at 0x004E0860
	CallVirtual<void(__thiscall*)(CGObjPC*, void*)>(this, 159)(this, pPacket);
}
void CGObjPC::SendWhisper(const char* senderName, const char* message)
{
	// 0x02 = CHAT_PM (private message) in the client's ChatType enum.
	// (0x09 was assumed to be "pink PM" but is CHAT_Stall in this client.)
	SendChatMessage(0x02, senderName, message);
}
uint16_t CGObjPC::AddItem(const char* Codename, int32_t Amount, bool RandomizeStats, int8_t OptLevel)
{
	int16_t operation_result = 0;
	CallVirtual<void(__thiscall*)(CGObjPC*, int32_t, const char*, int32_t, int16_t*, int8_t, int32_t, int32_t, int8_t, int32_t, int32_t, int8_t, int32_t)>(this, 139)(this, 0, Codename, Amount, &operation_result, 0, RandomizeStats ? 1 : 0, 0, OptLevel, 0, 1, 0, 0);
	return operation_result;
}
void CGObjPC::UpdateGold(int64_t Offset)
{
	// Check if offset is higher than int.MaxValue to avoid send a bugged message
	if(Offset > 0x7FFFFFFF)
		UpdateGold(Offset, 25, true, false);
	else
		UpdateGold(Offset, 25, true, true);
}
void CGObjPC::UpdateHwan(uint8_t Level)
{
	return CallVirtual<void(__thiscall*)(CGObjPC*, uint8_t)>(this, 197)(this, Level);
}
void CGObjPC::UpdateExperience(int64_t ExpOffset)
{
	CallVirtual<void(__thiscall*)(CGObjPC*, uint32_t, int64_t, uint32_t, std::uintptr_t*)>(this, 92)(this, m_UniqueId, ExpOffset, 0, nullptr);
}
void CGObjPC::AddSPExperience(uint32_t SPExpOffset)
{
	CallVirtual<void(__thiscall*)(CGObjPC*, uint32_t, int64_t, uint32_t, std::uintptr_t*)>(this, 92)(this, m_UniqueId, 0, SPExpOffset, nullptr);
}
void CGObjPC::UpdateSP(int32_t Offset)
{
	CallVirtual<void(__thiscall*)(CGObjPC*, int32_t, int8_t)>(this, 93)(this, Offset, 1);
}
void CGObjPC::ReduceHPMP(uint32_t Health, uint32_t Mana, bool ShowEffect)
{
	// Check if player will die by health reduction
	bool died = Health >= m_CInstancePC->Health;
	if (died)
	{
		Health = m_CInstancePC->Health;
		Mana = m_CInstancePC->Mana;
	}
	CallVirtual<void(__thiscall*)(CGObjPC*, uint32_t, uint32_t, uint16_t)>(this, 194)(this, Health, Mana, ShowEffect ? 1024 : 0);
	// Set dead status
	if (died)
		SetLifeState(false);
}
void CGObjPC::UpdatePVPCapeType(uint8_t CapeType)
{
	// Avoid unnecesary updates
	if (CapeType <= 5)
		reinterpret_cast<void(__thiscall*)(CGObjPC*, uint8_t)>(0x004F0E90)(this, CapeType);
}
bool CGObjPC::MoveTo(uint16_t RegionId, float PosX, float PosY, float PosZ)
{
	uint32_t gwid = 0;
	GetGameWorldId(&gwid);
	CPosition pos(RegionId, m_Position.UnkUShort01,PosX,PosY,PosZ);
	return MoveTo(gwid, pos, 2);
}
bool CGObjPC::MoveTo(uint32_t GameWorldId, uint16_t RegionId, float PosX, float PosY, float PosZ)
{
	CPosition pos(RegionId, m_Position.UnkUShort01, PosX, PosY, PosZ);
	return MoveTo(GameWorldId, pos, 2);
}
bool CGObjPC::MutateItemAt(uint8_t Slot,const char* NewCodename)
{
	return CallVirtual<bool(__thiscall*)(CGObjPC*, int32_t, const char*)>(this, 141)(this, Slot, NewCodename);
}
bool CGObjPC::Reload()
{
	uint32_t gwid = 0;
	GetGameWorldId(&gwid);
	return MoveTo(gwid, m_Position, 2);
}
void CGObjPC::ApplyGuildNickName(const char* Nickname)
{
	std::n_string* nick = new std::n_string(Nickname);
	uint32_t charId = m_CInstancePC->CharID;

	__asm
	{
		push nick;
		push 0x25; // Update type
		push this;

		mov ecx, charId;
		xor edx, edx;
		
		mov eax, 0x005C80A0;
		call eax;
	}
}
/* Private Helpers */
void CGObjPC::UpdateGold(int64_t Offset, int32_t Unknown, bool Realtime, bool ShowMessage)
{
	CallVirtual<void(__thiscall*)(CGObjPC*, int64_t, int32_t, int32_t, int32_t)>(this, 91)(this, Offset, Unknown, (Realtime ? 1 : 0), (ShowMessage ? 1 : 0));
}
bool CGObjPC::MoveTo(uint32_t GameWorldId, CPosition Position, int32_t Type)
{
	return CallVirtual<bool(__thiscall*)(CGObjPC*, uint32_t*, CPosition, int32_t)>(this, 222)(this, &GameWorldId, Position, Type);
}
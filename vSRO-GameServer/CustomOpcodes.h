#pragma once

#if defined(_MSC_VER) && (_MSC_VER < 1600)
// Visual Studio 2005 / 2008 (VC80 / VC90) compatibility
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
#else
#include <stdint.h>
#endif

// ============================================================================
// vSRO Custom Opcode Wire Architecture (Single Source of Truth)
// ============================================================================
// Bitmask Rules verified against AgentServer.exe (0x00401B24 & 0x00401B42):
// - C -> S: Must have Bit 12 (0x1000) set, Bit 15 (0x8000) clear, Bit 11 (0x0800) clear.
//           Valid Safe Block: 0x7500 - 0x77FF (specifically 0x7600 - 0x77FF).
// - S -> C Response (Ack): Opcode | 0x4000 (Bit 14 set). Safe Block: 0xB500 - 0xB7FF.
// - S -> C Broadcast (Push): Safe Block: 0x3500 - 0x37FF.
// ============================================================================

// Client -> Server Requests (0x7700 - 0x77FF)
enum CustomOpcodesCS
{
	OPCODE_CS_BULK_STAT_REQ     = 0x7710, // Payload: uint8_t statType (0 = STR, 1 = INT)
	OPCODE_CS_SKIN_SHOP_BUY_REQ = 0x7711, // Reserved for future skin shop
};

// Server -> Client Responses / Acks (0xB700 - 0xB7FF) [Request | 0x4000]
enum CustomOpcodesSC
{
	OPCODE_SC_BULK_STAT_ACK     = 0xB710, // Reserved (sync currently uses Joymax 0x303D + 0x304E)
	OPCODE_SC_SKIN_SHOP_BUY_ACK = 0xB711, // Reserved
};

// Server -> Client Push / Broadcasts (0x3700 - 0x37FF)
enum CustomOpcodesPush
{
	OPCODE_SC_CUSTOM_NOTIFY     = 0x3710, // Reserved
};

// Payload Values for OPCODE_CS_BULK_STAT_REQ
enum BulkStatType
{
	BULK_STAT_TYPE_STR = 0,
	BULK_STAT_TYPE_INT = 1,
};

#pragma once

#include <cstdint>
#include <string>
#include <map>
#include <vector>
#include "ZResource.h"
#include "ZFile.h"

#define SHORT_UPPERHALF(x)  (((x) >> 8) & 0xFF)
#define SHORT_LOWERHALF(x)  (((x) >> 0) & 0xFF)


enum class ZMessageEncoding
{
	Ascii, // TODO: think in a proper name
	Jpn,
	Cn,
	//Tw,
};

class ZMessage : public ZResource
{
protected:
	ZMessageEncoding encoding = ZMessageEncoding::Ascii;
	std::vector<uint8_t> u8Chars; // Ascii
	std::vector<uint16_t> u16Chars; // Jap
	
	size_t padding = 0;

public:
	ZMessage() = default;
	ZMessage(tinyxml2::XMLElement* reader, const std::vector<uint8_t>& nRawData, int nRawDataIndex,
	          ZFile* nParent);
	ZMessage(ZMessageEncoding nEncoding, const std::string& prefix,
	          const std::vector<uint8_t>& nRawData, int nRawDataIndex, ZFile* nParent);
	//~ZMessage();
	void ParseXML(tinyxml2::XMLElement* reader) override;
	void ParseRawData() override;
	static ZMessage* ExtractFromXML(tinyxml2::XMLElement* reader,
				const std::vector<uint8_t>& nRawData, int nRawDataIndex,
				std::string nRelPath, ZFile* nParent);
	//void Save(const std::string& outFolder) override;

	int GetRawDataSize() override;
	size_t GetRawDataSizeWithPadding();
	std::string GetSourceOutputCode(const std::string& prefix) override;

	std::string GetSourceTypeName() override;
	ZResourceType GetResourceType() override;

	size_t GetMessageLength();
	std::string GetCharacterAt(size_t index, size_t& codeSize);

	// Convenience method that calls GetAsciiMacro, GetJpnMacro or GetCnMacro.
	std::string GetMacro(size_t index, size_t& codeSize);
	std::string GetAsciiMacro(size_t index, size_t& codeSize);
	std::string GetJpnMacro(size_t index, size_t& codeSize);
	std::string GetCnMacro(size_t index, size_t& codeSize);

	std::string MakeMacroWithArguments(size_t u8Index, const std::pair<uint16_t, std::pair<const char*, size_t>>& macroData);
	static size_t GetMacroArgumentsPadding(uint16_t code, ZMessageEncoding encoding);

	static size_t GetBytesPerCode(uint16_t code, ZMessageEncoding encoding);

	bool IsLineBreak(size_t index);
	bool IsEndMarker(size_t index);
	static bool IsCodeLineBreak(uint16_t code, ZMessageEncoding encoding);
	static bool IsCodeEndMarker(uint16_t code, ZMessageEncoding encoding);
	static bool IsCodeTextColor(uint16_t code, ZMessageEncoding encoding);

	inline static const std::map<uint16_t, std::pair<const char*, size_t>> formatCodeMacrosAsciiOoT = {
		// { code, { "macro name", number of arguments (in bytes) } },
		// { 0x00, { "MSGCODE_GARBAGE_1", 0 } },
		{ 0x01, { "MSGCODE_LINEBREAK", 0 } },
		{ 0x02, { "MSGCODE_ENDMARKER", 0 } },
		// { 0x03, { "MSGCODE_GARBAGE_2", 0 } },
		{ 0x04, { "MSGCODE_BOXBREAK", 0 } },
		{ 0x05, { "MSGCODE_TEXTCOLOR", 1 } },
		{ 0x06, { "MSGCODE_INDENT", 1 } },
		{ 0x07, { "MSGCODE_NEXTMSGID", 2 } },
		{ 0x08, { "MSGCODE_INSTANT_ON", 0 } },
		{ 0x09, { "MSGCODE_INSTANT_OFF", 0 } },
		{ 0x0A, { "MSGCODE_KEEPOPEN", 0 } },
		{ 0x0B, { "MSGCODE_UNKEVENT", 0 } },
		{ 0x0C, { "MSGCODE_DELAY_BOXBREAK", 1 } },
		{ 0x0D, { "MSGCODE_UNUSED_1", 0 } }, // (Unused?) Wait for button press, continue in same box or line. 
		{ 0x0E, { "MSGCODE_DELAY_FADEOUT", 1 } },
		{ 0x0F, { "MSGCODE_PLAYERNAME", 0 } },
		{ 0x10, { "MSGCODE_BEGINOCARINA", 0 } },
		{ 0x11, { "MSGCODE_UNUSED_2", 0 } }, // (Unused?) Fade out and wait; ignore following text. 
		// { 0x11, { "MSGCODE_UNUSED_2", 1 } },
		{ 0x12, { "MSGCODE_PLAYSOUND", 2 } },
		{ 0x13, { "MSGCODE_ITEMICON", 1 } },
		{ 0x14, { "MSGCODE_TEXTSPEED", 1 } },
		{ 0x15, { "MSGCODE_BACKGROUND", 3 } },
		{ 0x16, { "MSGCODE_MARATHONTIME", 0 } },
		{ 0x17, { "MSGCODE_HORSERACETIME", 0 } },
		{ 0x18, { "MSGCODE_HORSEBACKARCHERYSCORE", 0 } },
		{ 0x19, { "MSGCODE_GOLDSKULLTULATOTAL", 0 } },
		{ 0x1A, { "MSGCODE_NOSKIP", 0 } },
		{ 0x1B, { "MSGCODE_TWOCHOICE", 0 } },
		{ 0x1C, { "MSGCODE_THREECHOICE", 0 } },
		{ 0x1D, { "MSGCODE_FISHSIZE", 0 } },
		{ 0x1E, { "MSGCODE_HIGHSCORE", 1 } },
		{ 0x1F, { "MSGCODE_TIME", 0 } },
	};
	inline static const std::map<uint16_t, std::pair<const char*, size_t>> formatCodeMacrosJpnOoT = {
		// { code, { "macro name", number of arguments (in bytes) } },
		// { 0x????, { "MSGCODE_GARBAGE_1", 0 } },
		{ 0x000A, { "MSGCODE_LINEBREAK", 0 } },
		{ 0x8170, { "MSGCODE_ENDMARKER", 0 } },
		// { 0x????, { "MSGCODE_GARBAGE_2", 0 } },
		{ 0x81A5, { "MSGCODE_BOXBREAK", 0 } },
		{ 0x000B, { "MSGCODE_TEXTCOLOR", 1 } },
		{ 0x86C7, { "MSGCODE_INDENT", 1 } },
		{ 0x81CB, { "MSGCODE_NEXTMSGID", 2 } },
		{ 0x8189, { "MSGCODE_INSTANT_ON", 0 } },
		{ 0x818A, { "MSGCODE_INSTANT_OFF", 0 } },
		{ 0x86C8, { "MSGCODE_KEEPOPEN", 0 } },
		{ 0x819F, { "MSGCODE_UNKEVENT", 0 } },
		{ 0x81A3, { "MSGCODE_DELAY_BOXBREAK", 1 } },
		//{ 0x????:, { "MSGCODE_UNUSED_1", 0 } }, // (Unused?) Wait for button press, continue in same box or line. 
		{ 0x819E, { "MSGCODE_DELAY_FADEOUT", 1 } },
		{ 0x874F, { "MSGCODE_PLAYERNAME", 0 } },
		{ 0x81F0, { "MSGCODE_BEGINOCARINA", 0 } },
		// { 0x????, { "MSGCODE_UNUSED_2", 0 } }, // (Unused?) Fade out and wait; ignore following text. 
		// { 0x????, { "MSGCODE_UNUSED_2", 1 } },
		{ 0x81F3, { "MSGCODE_PLAYSOUND", 2 } },
		{ 0x819A, { "MSGCODE_ITEMICON", 1 } },
		{ 0x86C9, { "MSGCODE_TEXTSPEED", 1 } },
		{ 0x86B3, { "MSGCODE_BACKGROUND", 3 } },
		{ 0x8791, { "MSGCODE_MARATHONTIME", 0 } },
		{ 0x8792, { "MSGCODE_HORSERACETIME", 0 } },
		{ 0x879B, { "MSGCODE_HORSEBACKARCHERYSCORE", 0 } },
		{ 0x86A3, { "MSGCODE_GOLDSKULLTULATOTAL", 0 } },
		{ 0x8199, { "MSGCODE_NOSKIP", 0 } },
		{ 0x81BC, { "MSGCODE_TWOCHOICE", 0 } },
		{ 0x81B8, { "MSGCODE_THREECHOICE", 0 } },
		{ 0x86A4, { "MSGCODE_FISHSIZE", 0 } },
		{ 0x869F, { "MSGCODE_HIGHSCORE", 1 } },
		{ 0x81A1, { "MSGCODE_TIME", 0 } },
	};
	inline static const std::map<uint16_t, const char*> specialCharactersOoT = {
		// { code, "macro name" },
		{ 0x96, "MSGCODE_E_ACUTE_LOWERCASE" }, // "é"

		{ 0x9F, "MSGCODE_A_BTN" },
		{ 0xA0, "MSGCODE_B_BTN" },
		{ 0xA1, "MSGCODE_C_BTN" },

		{ 0xA2, "MSGCODE_L_BTN" },
		{ 0xA3, "MSGCODE_R_BTN" },
		{ 0xA4, "MSGCODE_Z_BTN" },

		{ 0xA5, "MSGCODE_CUP_BTN" },
		{ 0xA6, "MSGCODE_CDOWN_BTN" },
		{ 0xA7, "MSGCODE_CLEFT_BTN" },
		{ 0xA8, "MSGCODE_CRIGHT_BTN" },

		{ 0xA9, "MSGCODE_TARGET_ICON" },
		{ 0xAA, "MSGCODE_STICK" },
		{ 0xAB, "MSGCODE_DPAD" }, // Unused.
	};
	inline static const std::map<uint16_t, const char*> colorMacrosOoT = {
		// { code, "macro name" },
		{ 0, "DEFAULT" },
		{ 1, "RED" },
		{ 2, "GREEN" },
		{ 3, "BLUE" },
		{ 4, "LIGHTBLUE" },
		{ 5, "PINK" },
		{ 6, "YELLOW" },
		{ 7, "WHITE" },
	};
};

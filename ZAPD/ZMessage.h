#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "ZResource.h"
#include "ZFile.h"


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
	std::string GetCharacterAt(size_t index, size_t& charSize);

	// Convenience method that calls GetAsciiMacro or GetJpnMacro.
	std::string GetMacro(size_t index, size_t& charSize);
	std::string GetAsciiMacro(size_t index, size_t& charSize);
	std::string GetJpnMacro(size_t index, size_t& charSize);
	std::string GetCnMacro(size_t index, size_t& charSize);

	static const char* GetSpecialCharacterMacro(uint8_t code);
	static const char* GetColorMacro(uint16_t code);
	static size_t GetBytesPerCode(uint16_t code, ZMessageEncoding encoding);

	bool IsLineBreak(size_t index);
	bool IsEndMarker(size_t index);
	static bool IsCodeLineBreak(uint16_t code, ZMessageEncoding encoding);
	static bool IsCodeEndMarker(uint16_t code, ZMessageEncoding encoding);
};

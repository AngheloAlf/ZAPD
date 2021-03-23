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
	//Cn,
	//Tw,
};

class ZMessage : public ZResource
{
protected:
	ZMessageEncoding encoding = ZMessageEncoding::Ascii;
	std::vector<uint8_t> u8Chars; // Ascii
	std::vector<uint16_t> u16Chars; // Jap
	/*ZSkeletonType type = ZSkeletonType::Normal;
	ZLimbType limbType = ZLimbType::Standard;
	std::vector<ZLimb*> limbs;
	segptr_t limbsArrayAddress;
	uint8_t limbCount;
	uint8_t dListCount;  // FLEX SKELETON ONLY*/

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
	std::string GetSourceOutputCode(const std::string& prefix) override;

	std::string GetSourceTypeName() override;
	ZResourceType GetResourceType() override;

	size_t GetMessageLength();
	std::string GetCharacterAt(size_t index, size_t& charSize);

	// Convenience method that calls GetAsciiMacro or GetJpnMacro.
	std::string GetMacro(size_t index, size_t& charSize);
	std::string GetAsciiMacro(size_t index, size_t& charSize);
	std::string GetJpnMacro(size_t index, size_t& charSize);

	static const char* GetColorMacro(uint16_t code);

	bool IsLineBreak(size_t index);
	bool IsEndMarker(size_t index);
};
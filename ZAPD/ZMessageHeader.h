#pragma once

#include <cstdint>
#include "ZResource.h"
#include "ZFile.h"
#include "ZMessage.h"


class ZMessageHeader : public ZResource
{
protected:
	ZMessageEncoding encoding = ZMessageEncoding::Ascii;
	ZMessage msg;

	uint8_t boxType;
	uint8_t yPos;
	uint16_t itemIcon;
	uint16_t nextMsgId;
	uint16_t itemPrice; // in rupees.
	uint16_t FFFF_1;
	uint16_t FFFF_2;

public:
	ZMessageHeader() = default;
	ZMessageHeader(tinyxml2::XMLElement* reader, const std::vector<uint8_t>& nRawData, int nRawDataIndex,
	          ZFile* nParent);
	ZMessageHeader(ZMessageEncoding nEncoding, const std::string& prefix,
	          const std::vector<uint8_t>& nRawData, int nRawDataIndex, ZFile* nParent);
	void ParseXML(tinyxml2::XMLElement* reader) override;
	void ParseRawData() override;
	static ZMessageHeader* ExtractFromXML(tinyxml2::XMLElement* reader,
				const std::vector<uint8_t>& nRawData, int nRawDataIndex,
				std::string nRelPath, ZFile* nParent);

	int GetRawDataSize() override;
	size_t GetRawDataSizeWithMessage();

	void DeclareVar(const std::string& prefix, const std::string& bodyStr);

	std::string GetBodySourceCode();
	std::string GetSourceOutputCode(const std::string& prefix) override;
	static std::string GetDefaultName(const std::string& prefix, uint32_t address);

	std::string GetSourceTypeName() override;
	ZResourceType GetResourceType() override;
};

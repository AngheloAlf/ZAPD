#include "ZMessageHeader.h"
#include "BitConverter.h"
#include "StringHelper.h"


ZMessageHeader::ZMessageHeader(tinyxml2::XMLElement* reader, const std::vector<uint8_t>& nRawData, int nRawDataIndex,
            ZFile* nParent)
{
	rawData.assign(nRawData.begin(), nRawData.end());
	rawDataIndex = nRawDataIndex;
	parent = nParent;

	ParseXML(reader);
	ParseRawData();

    msg = ZMessage(encoding, name, nRawData, nRawDataIndex + GetRawDataSize(), nParent);
}

ZMessageHeader::ZMessageHeader(ZMessageEncoding nEncoding, const std::string& prefix,
            const std::vector<uint8_t>& nRawData, int nRawDataIndex, ZFile* nParent)
{
	rawData.assign(nRawData.begin(), nRawData.end());
	rawDataIndex = nRawDataIndex;
	parent = nParent;

	name = GetDefaultName(prefix.c_str(), rawDataIndex);
	encoding = nEncoding;

	ParseRawData();

    msg = ZMessage(encoding, name, nRawData, nRawDataIndex + GetRawDataSize(), nParent);
}

void ZMessageHeader::ParseXML(tinyxml2::XMLElement* reader)
{
	ZResource::ParseXML(reader);

    // Optional (?)
	const char* encodingXml = reader->Attribute("Encoding");
	if (encodingXml != nullptr)
	{
        std::string encodingStr(encodingXml);
		if (encodingStr == "Ascii")
		{
			encoding = ZMessageEncoding::Ascii;
		}
		else if (encodingStr == "Jpn")
		{
			encoding = ZMessageEncoding::Jpn;
		}
		else if (encodingStr == "Cn")
		{
			encoding = ZMessageEncoding::Cn;
		}
		else
		{
			fprintf(stderr,
			        "ZMessageHeader::ParseXML: Warning in '%s'.\n\t Invalid Type found: '%s'. "
			        "Defaulting to 'Ascii'.\n",
			        name.c_str(), encodingXml);
			encoding = ZMessageEncoding::Ascii;
        }
	}
}

void ZMessageHeader::ParseRawData()
{
	ZResource::ParseRawData();

    boxType = BitConverter::ToUInt8BE(rawData, rawDataIndex + 0);
    yPos = BitConverter::ToUInt8BE(rawData, rawDataIndex + 1);

    if (encoding == ZMessageEncoding::Jpn)
    {
        itemIcon = BitConverter::ToUInt16BE(rawData, rawDataIndex + 2);
        nextMsgId = BitConverter::ToUInt16BE(rawData, rawDataIndex + 4);
        itemPrice = BitConverter::ToUInt16BE(rawData, rawDataIndex + 6);
        FFFF_1 = BitConverter::ToUInt16BE(rawData, rawDataIndex + 8);
        FFFF_2 = BitConverter::ToUInt16BE(rawData, rawDataIndex + 10);
    }
    else
    {
        itemIcon = BitConverter::ToUInt8BE(rawData, rawDataIndex + 2);
        nextMsgId = BitConverter::ToUInt16BE(rawData, rawDataIndex + 3);
        itemPrice = BitConverter::ToUInt16BE(rawData, rawDataIndex + 5);
        FFFF_1 = BitConverter::ToUInt16BE(rawData, rawDataIndex + 7);
        FFFF_2 = BitConverter::ToUInt16BE(rawData, rawDataIndex + 9);
    }
}

ZMessageHeader* ZMessageHeader::ExtractFromXML(tinyxml2::XMLElement* reader,
            const std::vector<uint8_t>& nRawData, int nRawDataIndex,
            std::string nRelPath, ZFile* nParent)
{
	ZMessageHeader* msgHeader = new ZMessageHeader(reader, nRawData, nRawDataIndex, nParent);
	msgHeader->relativePath = std::move(nRelPath);

    msgHeader->DeclareVar("", "");

	return msgHeader;
}

int ZMessageHeader::GetRawDataSize()
{
    if (encoding == ZMessageEncoding::Jpn)
    {
        return 0x0C;
    }
    return 0x0B;
}

size_t ZMessageHeader::GetRawDataSizeWithMessage()
{
    return GetRawDataSize() + msg.GetRawDataSizeWithPadding();
}

void ZMessageHeader::DeclareVar(const std::string& prefix, const std::string& bodyStr)
{
    std::string auxName = name;
    if (name == "")
    {
        auxName = GetDefaultName(prefix, rawDataIndex);
    }
    parent->AddDeclaration(rawDataIndex, DeclarationAlignment::None, GetRawDataSize(),
                            GetSourceTypeName(), auxName, bodyStr);
}

std::string ZMessageHeader::GetBodySourceCode()
{
	std::string bodyStr = "";

    bodyStr += StringHelper::Sprintf("0x%02X, 0x%02X, ", boxType, yPos);
    if (encoding == ZMessageEncoding::Jpn)
    {
        bodyStr += StringHelper::Sprintf("0x%04X, ", itemIcon);
    }
    else
    {
        bodyStr += StringHelper::Sprintf("0x%02X, ", itemIcon);
    }
    bodyStr += StringHelper::Sprintf("\n    0x%04X, 0x%04X, ", nextMsgId, itemPrice);
    bodyStr += StringHelper::Sprintf("\n    0x%04X, 0x%04X, ", FFFF_1, FFFF_2);

    return bodyStr;
}

std::string ZMessageHeader::GetSourceOutputCode(const std::string& prefix)
{
    msg.GetSourceOutputCode(prefix);

    std::string bodyStr = "\n    " + GetBodySourceCode() + "\n";

	Declaration* decl = parent->GetDeclaration(rawDataIndex);
	if (decl == nullptr)
	{
        DeclareVar(prefix, bodyStr);
	}
	else
	{
		decl->text = bodyStr;
	}

    return "";
}

std::string ZMessageHeader::GetDefaultName(const std::string& prefix, uint32_t address)
{
    return StringHelper::Sprintf("%sMsgHeader_%06X", prefix.c_str(), address);
}

std::string ZMessageHeader::GetSourceTypeName()
{
    if (encoding == ZMessageEncoding::Jpn)
    {
        return "MessageHeaderJpn";
    }
    return "MessageHeader";
}

ZResourceType ZMessageHeader::GetResourceType()
{
    return ZResourceType::MessageHeader;
}

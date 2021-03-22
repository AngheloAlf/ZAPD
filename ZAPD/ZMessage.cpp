#include "ZMessage.h"
#include "BitConverter.h"
#include "StringHelper.h"


ZMessage::ZMessage(tinyxml2::XMLElement* reader, const std::vector<uint8_t>& nRawData, int nRawDataIndex,
	          ZFile* nParent)
{
	rawData.assign(nRawData.begin(), nRawData.end());
	rawDataIndex = nRawDataIndex;
	parent = nParent;

	ParseXML(reader);
	ParseRawData();
}

ZMessage::ZMessage(ZMessageEncoding nEncoding, const std::string& prefix,
            const std::vector<uint8_t>& nRawData, int nRawDataIndex, ZFile* nParent)
{
	rawData.assign(nRawData.begin(), nRawData.end());
	rawDataIndex = nRawDataIndex;
	parent = nParent;

	name = StringHelper::Sprintf("%sMessage_%06X", prefix.c_str(), rawDataIndex);
	encoding = nEncoding;

	ParseRawData();
}

void ZMessage::ParseXML(tinyxml2::XMLElement* reader)
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
		else if (encodingStr == "Jap")
		{
			encoding = ZMessageEncoding::Jap;
		}
		else
		{
			fprintf(stderr,
			        "ZMessage::ParseXML: Warning in '%s'.\n\t Invalid Type found: '%s'. "
			        "Defaulting to 'Ascii'.\n",
			        name.c_str(), encodingXml);
			encoding = ZMessageEncoding::Ascii;
        }
	}
}

void ZMessage::ParseRawData()
{
	ZResource::ParseRawData();

    if (encoding == ZMessageEncoding::Ascii) {
        for (size_t i = 0; ; i++) {
            uint8_t c = rawData.at(rawDataIndex + i);
            u8Chars.push_back(c);
            if (c == 0x02) { // End Marker
                break;
            }
        }
    }
    else if (encoding == ZMessageEncoding::Jap) {
        for (size_t i = 0; ; i++) {
            uint16_t c = BitConverter::ToUInt16BE(rawData, rawDataIndex + i * 2);
            u16Chars.push_back(c);
            if (c == 0x8170) { // End Marker
                break;
            }
        }
    }
}

ZMessage* ZMessage::ExtractFromXML(tinyxml2::XMLElement* reader,
            const std::vector<uint8_t>& nRawData, int nRawDataIndex,
            std::string nRelPath, ZFile* nParent)
{
	ZMessage* message = new ZMessage(reader, nRawData, nRawDataIndex, nParent);
	message->relativePath = std::move(nRelPath);

	message->parent->AddDeclarationArray(message->rawDataIndex, DeclarationAlignment::None,
	                                 message->GetRawDataSize(), message->GetSourceTypeName(),
	                                 message->name, message->GetMessageLength(), "");

	return message;
}

int ZMessage::GetRawDataSize()
{
    switch (encoding)
    {
    case ZMessageEncoding::Ascii:
        return u8Chars.size();
    case ZMessageEncoding::Jap:
        return u16Chars.size() * 2;
    }
    return 0;
}

std::string ZMessage::GetSourceOutputCode(const std::string& prefix)
{
	std::string bodyStr = "";
    
    switch (encoding)
    {
    case ZMessageEncoding::Ascii:
        bodyStr = StringHelper::Sprintf("    \"%.*s\"", u8Chars.size(), u8Chars.data());
    case ZMessageEncoding::Jap:
        //return u16Chars.size() * 2;
        break;
    }

	Declaration* decl = parent->GetDeclaration(rawDataIndex);
	if (decl == nullptr)
	{
		parent->AddDeclarationArray(rawDataIndex, DeclarationAlignment::None, GetRawDataSize(),
		                       GetSourceTypeName(), name, GetMessageLength(), bodyStr);
	}
	else
	{
		decl->text = bodyStr;
	}

    return "";
}

std::string ZMessage::GetSourceTypeName()
{
    switch (encoding)
    {
    case ZMessageEncoding::Ascii:
        return "char*";
    case ZMessageEncoding::Jap:
        throw std::runtime_error("TODO");
    }
    return "ERROR";
}

ZResourceType ZMessage::GetResourceType()
{
    return ZResourceType::Message;
}

size_t ZMessage::GetMessageLength()
{
    switch (encoding)
    {
    case ZMessageEncoding::Ascii:
        return u8Chars.size();
    //case ZMessageEncoding::Jap: // TODO
    //    return u16Chars.size() * 2;
    }
    return 0;
}

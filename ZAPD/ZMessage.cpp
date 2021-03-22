#include "ZMessage.h"
#include "BitConverter.h"
#include "StringHelper.h"
#include <locale>
#include <codecvt>


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
		else if (encodingStr == "Jpn")
		{
			encoding = ZMessageEncoding::Jpn;
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

    for (size_t i = 0; ; i+=2) {
        uint8_t c = rawData.at(rawDataIndex + i);
        uint8_t c2 = rawData.at(rawDataIndex + i + 1);
        uint16_t wide = BitConverter::ToUInt16BE(rawData, rawDataIndex + i);
        u8Chars.push_back(c);
        if (encoding == ZMessageEncoding::Ascii && c == 0x02) { // End Marker
            break;
        }
        u16Chars.push_back(wide);
        u8Chars.push_back(c2);
        if (encoding == ZMessageEncoding::Jpn && wide == 0x8170) { // End Marker
            break;
        }
        if (encoding == ZMessageEncoding::Ascii && c2 == 0x02) { // End Marker
            break;
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
	                                 message->name, message->GetRawDataSize(), "");

	return message;
}

int ZMessage::GetRawDataSize()
{
    return u8Chars.size();
}

std::string ZMessage::GetSourceOutputCode(const std::string& prefix)
{
	std::string bodyStr = "";

    bodyStr = "    \"";

    size_t i = 0;
    while (i < GetMessageLength())
    {
        size_t charSize = 0;
        bodyStr += GetCharacterAt(i, charSize);
        i += charSize;
    }

    bodyStr += "\"";

	Declaration* decl = parent->GetDeclaration(rawDataIndex);
	if (decl == nullptr)
	{
		parent->AddDeclarationArray(rawDataIndex, DeclarationAlignment::None, GetRawDataSize(),
		                       GetSourceTypeName(), name, GetRawDataSize(), bodyStr);
	}
	else
	{
		decl->text = bodyStr;
	}

    return "";
}

std::string ZMessage::GetSourceTypeName()
{
    return "char*";
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
    case ZMessageEncoding::Jpn:
        return u16Chars.size();
    }
    return 0;
}

std::string ZMessage::GetCharacterAt(size_t index, size_t& charSize)
{
    std::string result = "";

    if (encoding == ZMessageEncoding::Ascii)
    {
        // TODO
        charSize = 1;
        uint8_t code = u8Chars.at(index);
        switch (code)
        {
        case 0x01:
            return "\" \"\\x01\" // MSGCODE_LINEBREAK\n    \"";
        case 0x02:
            return "\" \"\\x02\" // MSGCODE_ENDMARKER\n    \"";
        case 0x05:
            charSize = 2;
            return StringHelper::Sprintf("\" \"\\x%02X\\x%02X\" // MSGCODE_COLOR(0x%02X)\n    \"", code, u8Chars.at(index + 1), u8Chars.at(index + 1));
        case 0x06:
            charSize = 2;
            return StringHelper::Sprintf("\" \"\\x%02X\\x%02X\" // \n    \"", code, u8Chars.at(index + 1));
        case 0x07:
            charSize = 3;
            return StringHelper::Sprintf("\" \"\\x%02X\\x%02X\\x%02X\" // \n    \"", code, u8Chars.at(index + 1), u8Chars.at(index + 2));
        case 0x0C:
        case 0x0E:
            charSize = 2;
            return StringHelper::Sprintf("\" \"\\x%02X\\x%02X\" // \n    \"", code, u8Chars.at(index + 1));
        case 0x12:
            charSize = 3;
            return StringHelper::Sprintf("\" \"\\x%02X\\x%02X\\x%02X\" // \n    \"", code, u8Chars.at(index + 1), u8Chars.at(index + 2));
        case 0x13:
        case 0x14:
            charSize = 2;
            return StringHelper::Sprintf("\" \"\\x%02X\\x%02X\" // \n    \"", code, u8Chars.at(index + 1));
        case 0x15:
            charSize = 4;
            return StringHelper::Sprintf("\" \"\\x%02X\\x%02X\\x%02X\\x%02X\" // \n    \"", code, u8Chars.at(index + 1), u8Chars.at(index + 2), u8Chars.at(index + 3));
        case 0x1E:
            charSize = 2;
            return StringHelper::Sprintf("\" \"\\x%02X\\x%02X\" // \n    \"", code, u8Chars.at(index + 1));
        }
        if (code <= 0x1F)
        {
            return StringHelper::Sprintf("\" \"\\x%02X\" // \n    \"", code);
        }
        result += code;
        return result;
    }

    else if (encoding == ZMessageEncoding::Jpn)
    {
        charSize = 1;
        result += StringHelper::Sprintf("\\x%02X", u8Chars.at(2 * index));
        result += StringHelper::Sprintf("\\x%02X", u8Chars.at(2 * index + 1));
        return result;
        // TODO
        /*
        uint16_t wide = u16Chars.at(index);
        charSize = 1;
        std::wstring wResult;

        switch(wide)
        {
        case 0x000A:
        case 0x8170:
        case 0x81A5:
            return StringHelper::Sprintf("\\x%X", wide);

        case 0x000B: // xxxx:
            charSize = 2;
            return StringHelper::Sprintf("\\x%X\\x%X", wide, u16Chars.at(index+1));

        case 0x86C7: // 00xx:
            charSize = 2;
            return StringHelper::Sprintf("\\x%X\\x%X", wide, u16Chars.at(index+1));

        case 0x81CB: // xxxx:
            charSize = 2;
            return StringHelper::Sprintf("\\x%X\\x%X", wide, u16Chars.at(index+1));

        case 0x8189:
        case 0x818A:
        case 0x86C8:
        case 0x819F:
            return StringHelper::Sprintf("\\x%X", wide);

        case 0x81A3: // 00xx:
            charSize = 2;
            return StringHelper::Sprintf("\\x%X\\x%X", wide, u16Chars.at(index+1));

        //case 0x????:
            //return StringHelper::Sprintf("\\x%X", wide);

        case 0x819E: // 00xx:
            charSize = 2;
            return StringHelper::Sprintf("\\x%X\\x%X", wide, u16Chars.at(index+1));

        case 0x874F:
        case 0x81F0:
            return StringHelper::Sprintf("\\x%X", wide);

        //case 0x????:
            //return StringHelper::Sprintf("\\x%X", wide);

        case 0x81F3: // xxxx:
        case 0x819A: // 00xx:
        case 0x86C9: // 00xx:
            charSize = 2;
            return StringHelper::Sprintf("\\x%X\\x%X", wide, u16Chars.at(index+1));

        case 0x86B3: // 00xx xxxx:
            charSize = 3;
            return StringHelper::Sprintf("\\x%X\\x%X", wide, u16Chars.at(index+1), u16Chars.at(index+2));

        case 0x8791:
        case 0x8792:
        case 0x879B:
        case 0x86A3:
        case 0x8199:
        case 0x81BC:
        case 0x81B8:
        case 0x86A4:
            return StringHelper::Sprintf("\\x%X", wide);

        case 0x869F: // 00xx:
            charSize = 2;
            return StringHelper::Sprintf("\\x%X\\x%X", wide, u16Chars.at(index+1));

        case 0x81A1:
            return StringHelper::Sprintf("\\x%X", wide);

        default:
            wResult += wide;
            using convert_type = std::codecvt_utf8<wchar_t>;
            std::wstring_convert<convert_type, wchar_t> converter;

            //use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
            return converter.to_bytes( wResult );
        }
        */
    }

    return "ERROR";
}

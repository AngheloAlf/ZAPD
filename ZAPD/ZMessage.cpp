#include "ZMessage.h"
#include "BitConverter.h"
#include "StringHelper.h"
#include <locale>
#include <codecvt>
#include <cassert>


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
        result = GetAsciiMacro(index, charSize);
        if (charSize != 0) 
        {
            return StringHelper::Sprintf("\" %s \n    \"", result.c_str());
        }

        charSize = 1;
        result = "";
        uint8_t code = u8Chars.at(index);
        
        result += code;
        return result;
    }

    if (encoding == ZMessageEncoding::Jpn)
    {
        result = GetJpnMacro(index, charSize);
        if (charSize != 0) 
        {
            return StringHelper::Sprintf("\" %s \n    \"", result.c_str());
        }

        charSize = 1;
        result = "";

        result += u8Chars.at(2 * index);
        result += u8Chars.at(2 * index + 1);
        return result;
    }

    return "ERROR";
}


std::string ZMessage::GetAsciiMacro(size_t index, size_t& charSize)
{
    assert(encoding == ZMessageEncoding::Ascii);

    charSize = 1;
    uint8_t code = u8Chars.at(index);
    switch (code)
    {
    case 0x01:
        return "MSGCODE_LINEBREAK";
    case 0x02:
        return "MSGCODE_ENDMARKER";
    case 0x04:
        return "MSGCODE_BOXBREAK";
    case 0x05:
        charSize = 2;
        return StringHelper::Sprintf("MSGCODE_TEXTCOLOR(\"\\x%02X\")", u8Chars.at(index + 1));
    case 0x06:
        charSize = 2;
        return StringHelper::Sprintf("MSGCODE_INDENT(\"\\x%02X\")", u8Chars.at(index + 1));
    case 0x07:
        charSize = 3;
        return StringHelper::Sprintf("MSGCODE_NEXTMSGID(\"\\x%02X\", \"\\x%02X\")", u8Chars.at(index + 1), u8Chars.at(index + 2));
    case 0x08:
        return "MSGCODE_INSTANT_ON";
    case 0x09:
        return "MSGCODE_INSTANT_OFF";
    case 0x0A:
        return "MSGCODE_KEEPOPEN";
    case 0x0B:
        return "MSGCODE_UNKEVENT";
    case 0x0C:
        charSize = 2;
        return StringHelper::Sprintf("MSGCODE_DELAY_BOXBREAK(\"\\x%02X\")", u8Chars.at(index + 1));
    case 0x0D:
        return "MSGCODE_UNUSED_1"; // (Unused?) Wait for button press, continue in same box or line. 
    case 0x0E:
        charSize = 2;
        return StringHelper::Sprintf("MSGCODE_DELAY_FADEOUT(\"\\x%02X\")", u8Chars.at(index + 1));
    case 0x0F:
        return "MSGCODE_PLAYERNAME";
    case 0x10:
        return "MSGCODE_BEGINOCARINA";
    case 0x11:
        return "MSGCODE_UNUSED_2"; // (Unused?) Fade out and wait; ignore following text. 
    case 0x12:
        charSize = 3;
        return StringHelper::Sprintf("MSGCODE_PLAYSOUND(\"\\x%02X\", \"\\x%02X\")", u8Chars.at(index + 1), u8Chars.at(index + 2));
    case 0x13:
        charSize = 2;
        return StringHelper::Sprintf("MSGCODE_ITEMICON(\"\\x%02X\")", u8Chars.at(index + 1));
    case 0x14:
        charSize = 2;
        return StringHelper::Sprintf("MSGCODE_TEXTSPEED(\"\\x%02X\")", u8Chars.at(index + 1));
    case 0x15:
        charSize = 4;
        return StringHelper::Sprintf("MSGCODE_BACKGROUND(\"\\x%02X\", \\x%02X\", \"\\x%02X\")", u8Chars.at(index + 1), u8Chars.at(index + 2), u8Chars.at(index + 3));
    case 0x16:
        return "MSGCODE_MARATHONTIME";
    case 0x17:
        return "MSGCODE_HORSERACETIME";
    case 0x18:
        return "MSGCODE_HORSEBACKARCHERYSCORE";
    case 0x19:
        return "MSGCODE_GOLDSKULLTULATOTAL";
    case 0x1A:
        return "MSGCODE_NOSKIP";
    case 0x1B:
        return "MSGCODE_TWOCHOICE";
    case 0x1C:
        return "MSGCODE_THREECHOICE";
    case 0x1D:
        return "MSGCODE_FISHSIZE";
    case 0x1E:
        charSize = 2;
        return StringHelper::Sprintf("MSGCODE_HIGHSCORE(\"\\x%02X\")", u8Chars.at(index + 1));
    case 0x1F:
        return "MSGCODE_TIME";
    }

    charSize = 0;
    return "";
}

std::string ZMessage::GetJpnMacro(size_t index, size_t& charSize)
{
    assert(encoding == ZMessageEncoding::Jpn);

    charSize = 1;
    uint16_t code = u16Chars.at(index);
    uint16_t aux1, aux2, aux3;

    switch (code)
    {
    // CODES

    case 0x000A:
        return "MSGCODE_LINEBREAK";
    case 0x8170:
        return "MSGCODE_ENDMARKER";
    case 0x81A5:
        return "MSGCODE_BOXBREAK";
    case 0x000B:
        charSize = 2;
        return StringHelper::Sprintf("MSGCODE_TEXTCOLOR(\"\\x%02X\")", u16Chars.at(index + 1) & 0x0F);
    case 0x86C7:
        charSize = 2;
        return StringHelper::Sprintf("MSGCODE_INDENT(\"\\x%02X\")", u16Chars.at(index + 1));
    case 0x81CB:
        charSize = 2;
        aux1 = (u16Chars.at(index + 1) >> 8) & 0xFF;
        aux2 = (u16Chars.at(index + 1) >> 0) & 0xFF;
        return StringHelper::Sprintf("MSGCODE_NEXTMSGID(\"\\x%02X\", \"\\x%02X\")", aux1, aux2);
    case 0x8189:
        return "MSGCODE_INSTANT_ON";
    case 0x818A:
        return "MSGCODE_INSTANT_OFF";
    case 0x86C8:
        return "MSGCODE_KEEPOPEN";
    case 0x819F:
        return "MSGCODE_UNKEVENT";
    case 0x81A3:
        charSize = 2;
        return StringHelper::Sprintf("MSGCODE_DELAY_BOXBREAK(\"\\x%02X\")", u16Chars.at(index + 1));
    //case 0x????:
    //    return "MSGCODE_UNUSED_1"; // (Unused?) Wait for button press, continue in same box or line. 
    case 0x819E:
        charSize = 2;
        return StringHelper::Sprintf("MSGCODE_DELAY_FADEOUT(\"\\x%02X\")", u16Chars.at(index + 1));
    case 0x874F:
        return "MSGCODE_PLAYERNAME";
    case 0x81F0:
        return "MSGCODE_BEGINOCARINA";
    //case 0x????:
    //    return "MSGCODE_UNUSED_2"; // (Unused?) Fade out and wait; ignore following text. 
    case 0x81F3:
        charSize = 2;
        aux1 = (u16Chars.at(index + 1) >> 8) & 0xFF;
        aux2 = (u16Chars.at(index + 1) >> 0) & 0xFF;
        return StringHelper::Sprintf("MSGCODE_PLAYSOUND(\"\\x%02X\", \"\\x%02X\")", aux1, aux2);
    case 0x819A:
        charSize = 2;
        return StringHelper::Sprintf("MSGCODE_ITEMICON(\"\\x%02X\")", u16Chars.at(index + 1));
    case 0x86C9:
        charSize = 2;
        return StringHelper::Sprintf("MSGCODE_TEXTSPEED(\"\\x%02X\")", u16Chars.at(index + 1));
    case 0x86B3:
        charSize = 3;
        aux1 = (u16Chars.at(index + 1) >> 0) & 0xFF;
        aux2 = (u16Chars.at(index + 2) >> 8) & 0xFF;
        aux3 = (u16Chars.at(index + 2) >> 0) & 0xFF;
        return StringHelper::Sprintf("MSGCODE_BACKGROUND(\"\\x%02X\", \"\\x%02X\", \"\\x%02X\")", aux1, aux2, aux3);
    case 0x8791:
        return "MSGCODE_MARATHONTIME";
    case 0x8792:
        return "MSGCODE_HORSERACETIME";
    case 0x879B:
        return "MSGCODE_HORSEBACKARCHERYSCORE";
    case 0x86A3:
        return "MSGCODE_GOLDSKULLTULATOTAL";
    case 0x8199:
        return "MSGCODE_NOSKIP";
    case 0x81BC:
        return "MSGCODE_TWOCHOICE";
    case 0x81B8:
        return "MSGCODE_THREECHOICE";
    case 0x86A4:
        return "MSGCODE_FISHSIZE";
    case 0x869F:
        charSize = 2;
        return StringHelper::Sprintf("MSGCODE_HIGHSCORE(\"\\x%02X\")", u16Chars.at(index + 1));
    case 0x81A1:
        return "MSGCODE_TIME";

    // Special characters
    case 0x839F:
        return "MSGCODE_A_BTN";
    case 0x83A0:
        return "MSGCODE_B_BTN";
    case 0x83A1:
        return "MSGCODE_C_BTN";
    case 0x83A2:
        return "MSGCODE_L_BTN";
    case 0x83A3:
        return "MSGCODE_R_BTN";
    case 0x83A4:
        return "MSGCODE_Z_BTN";
    case 0x83A5:
        return "MSGCODE_CUP_BTN";
    case 0x83A6:
        return "MSGCODE_CDOWN_BTN";
    case 0x83A7:
        return "MSGCODE_CLEFT_BTN";
    case 0x83A8:
        return "MSGCODE_CRIGHT_BTN";
    case 0x83A9:
        return "MSGCODE_TARGET_ICON";
    case 0x83AA:
        return "MSGCODE_STICK";
    case 0x83AB:
        return "MSGCODE_DPAD";
    }

    charSize = 0;
    return "";
}

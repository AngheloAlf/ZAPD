#include "ZMessage.h"
#include "BitConverter.h"
#include "StringHelper.h"
#include <cassert>
#include "Globals.h"


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
		else if (encodingStr == "Cn")
		{
			encoding = ZMessageEncoding::Cn;
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

    size_t i = 0;
    if (encoding == ZMessageEncoding::Ascii || encoding == ZMessageEncoding::Cn)
    {
        while (true)
        {
            uint8_t code = rawData.at(rawDataIndex + i);

            if (IsCodeEndMarker(code, encoding))
            {
                u8Chars.push_back(code);
                break;
            }

            size_t bytes = GetBytesPerCode(code, encoding);
            while (bytes-- > 0)
            {
                u8Chars.push_back(rawData.at(rawDataIndex + i));
                i++;
            }
        }

        while ((rawDataIndex + i+1) % 4 != 0) {
            uint8_t code = rawData.at(rawDataIndex + i + 1);

            if (code != 0x00)
            {
                break;
            }
            padding += 1;
            i += 1;
        }

        for (size_t j = 0; j <  u8Chars.size(); j+=2)
        {
            u16Chars.push_back(BitConverter::ToUInt16BE(u8Chars, j));
        }
    }
    else if (encoding == ZMessageEncoding::Jpn)
    {
        while (true)
        {
            uint16_t code = BitConverter::ToUInt16BE(rawData, rawDataIndex + i);

            if (IsCodeEndMarker(code, encoding))
            {
                u16Chars.push_back(code);
                break;
            }

            size_t bytes = GetBytesPerCode(code, encoding);
            while (bytes > 0)
            {
                u16Chars.push_back(BitConverter::ToUInt16BE(rawData, rawDataIndex + i));
                i += 2;
                bytes -= 2;
            }
        }

        while ((rawDataIndex + i+2) % 4 != 0) {
            uint16_t code = BitConverter::ToUInt16BE(rawData, rawDataIndex + i + 2);

            if (code != 0x0000)
            {
                break;
            }
            padding += 2;
            i += 2;
        }

        for (size_t j = 0; j < u16Chars.size(); j++)
        {
            uint16_t code = u16Chars.at(j);
            u8Chars.push_back((code >> 8) & 0xFF);
            u8Chars.push_back((code >> 0) & 0xFF);
        }
    }
}

ZMessage* ZMessage::ExtractFromXML(tinyxml2::XMLElement* reader,
            const std::vector<uint8_t>& nRawData, int nRawDataIndex,
            std::string nRelPath, ZFile* nParent)
{
	ZMessage* message = new ZMessage(reader, nRawData, nRawDataIndex, nParent);
	message->relativePath = std::move(nRelPath);


    std::string auxName = message->GetName();
    if (auxName == "")
    {
        auxName = StringHelper::Sprintf("%sMsg_%06X", "", nRawDataIndex);
    }
	message->parent->AddDeclarationArray(message->rawDataIndex, DeclarationAlignment::None,
	                                 message->GetRawDataSize(), message->GetSourceTypeName(),
	                                 auxName, message->GetRawDataSize(), "");

	return message;
}

int ZMessage::GetRawDataSize()
{
    return u8Chars.size();
}

size_t ZMessage::GetRawDataSizeWithPadding()
{
    return u8Chars.size() + padding;
}

std::string ZMessage::GetSourceOutputCode(const std::string& prefix)
{
	std::string bodyStr = "    ";

    bool lastWasMacro = true;
    size_t i = 0;
    while (i < GetMessageLength())
    {
        size_t charSize = 0;
        std::string macro = GetMacro(i, charSize);

        if (charSize != 0) // It's a macro
        {
            if (!lastWasMacro)
            {
                bodyStr += "\" ";
                lastWasMacro = true;
            }
            bodyStr += macro + " ";
        }
        else
        {
            if (lastWasMacro)
            {
                if (i != 0)
                {
                    bodyStr += "\n    \"";
                }
                else
                {
                    bodyStr += "\"";
                }
                
                lastWasMacro = false;
            }
            bodyStr += GetCharacterAt(i, charSize);
            assert(charSize != 0);
        }
        i += charSize;
    }

    if (!lastWasMacro)
    {
        bodyStr += "\"";
    }

	Declaration* decl = parent->GetDeclaration(rawDataIndex);
	if (decl == nullptr)
	{
        std::string auxName = name;
        if (name == "")
        {
            auxName = StringHelper::Sprintf("%sMsg_%06X", prefix.c_str(), rawDataIndex);
        }
		parent->AddDeclarationArray(rawDataIndex, DeclarationAlignment::None, GetRawDataSize(),
		                       GetSourceTypeName(), auxName, GetRawDataSize(), bodyStr);
	}
	else
	{
		decl->text = bodyStr;
	}

    return "";
}

std::string ZMessage::GetSourceTypeName()
{
    return "char";
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
    case ZMessageEncoding::Cn:
        return u8Chars.size();
    case ZMessageEncoding::Jpn:
        return u16Chars.size();
    }
    return 0;
}

std::string ZMessage::GetCharacterAt(size_t index, size_t& codeSize)
{
    std::string result = "";

    if (encoding == ZMessageEncoding::Ascii)
    {
        codeSize = 1;
        uint8_t code = u8Chars.at(index);

        if (code == 0)
        {
            result += "\\0";
        }
        else if (code == '\"')
        {
            result += "\\\"";
        }
        else
        {
            result += code;
        }
        
        return result;
    }

    if (encoding == ZMessageEncoding::Jpn)
    {
        uint16_t code = u16Chars.at(index);
        codeSize = 1;

        if (code == 0x0000)
        {
            result += "\\0";
            result += "\\0";
        }
        /*else if (code == 0x835C)
        {
            // For some reason, the compiler will not omit the 0x53 part of this character.
            // So now it is a special
            result += "\\x83\\x5C"; // ソ
        }*/
        else
        {
            result += u8Chars.at(2 * index);
            result += u8Chars.at(2 * index + 1);
        }

        return result;
    }

    if (encoding == ZMessageEncoding::Cn)
    {
        codeSize = 1;
        uint8_t code = u8Chars.at(index);

        if (code == 0)
        {
            result += "\\0";
        }
        else if (code == '\"')
        {
            result += "\\\"";
        }
        else if (code >= 0xA0 && code <= 0xA7)
        {
            // If `code` is between 0xA0 and 0xA7, then it is a two bytes character

            uint8_t code2 = u8Chars.at(index + 1);
            codeSize = 2;

            // Commented until we figure out what is the encoding used.
            // result += code;
            // result += code2;

            result = StringHelper::Sprintf("\\x%02X\\x%02X", code, code2);
        }
        else
        {
            result += code;
        }
        
        return result;
    }

    return "ERROR";
}

std::string ZMessage::GetMacro(size_t index, size_t& codeSize)
{
    switch (encoding)
    {
    case ZMessageEncoding::Ascii:
        return GetAsciiMacro(index, codeSize);
    case ZMessageEncoding::Jpn:
        return GetJpnMacro(index, codeSize);
    case ZMessageEncoding::Cn:
        return GetCnMacro(index, codeSize);
    default:
        return "ERROR";
    }
}

std::string ZMessage::GetAsciiMacro(size_t index, size_t& codeSize)
{
    assert(encoding == ZMessageEncoding::Ascii);

    codeSize = 1;
    uint8_t code = u8Chars.at(index);

    // Special characters
    const auto& specialChar = specialCharactersOoT.find(code);
    if (specialChar != specialCharactersOoT.end())
    {
        return specialChar->second;
    }

    // Codes
    const auto& macroData = formatCodeMacrosAsciiOoT.find(code);
    if (macroData != formatCodeMacrosAsciiOoT.end())
    {
        size_t macroParams = macroData->second.second;
        codeSize += macroParams;
        return MakeMacroWithArguments(index+1, *macroData);
    }

    codeSize = 0;
    return "";
}

std::string ZMessage::GetJpnMacro(size_t index, size_t& codeSize)
{
    assert(encoding == ZMessageEncoding::Jpn);

    codeSize = 1;
    uint16_t code = u16Chars.at(index);
    uint16_t upperHalf = ((code >> 8) & 0xFF);

    // Special characters
    if (upperHalf == 0x83)
    {
        const auto& specialChar = specialCharactersOoT.find(code & 0xFF);
        if (specialChar != specialCharactersOoT.end())
        {
            return specialChar->second;
        }
    }

    // Codes
    const auto& macroData = formatCodeMacrosJpnOoT.find(code);
    if (macroData != formatCodeMacrosJpnOoT.end())
    {
        size_t macroParams = macroData->second.second;
        // Params amount is in bytes, but we want halfs (or 2bytes).
        codeSize += (macroParams+1) / 2;
        return MakeMacroWithArguments(2 * (index + 1), *macroData);
    }

    codeSize = 0;
    return "";
}

std::string ZMessage::GetCnMacro(size_t index, size_t& codeSize)
{
    assert(encoding == ZMessageEncoding::Cn);

    codeSize = 1;
    uint8_t code = u8Chars.at(index);

    // Special characters
    /**
     * A special character in the iQue releases has the format
     * `0xAASS`, where `SS` are the same special characters used
     * in non-jp releases.
     **/
    if (code == 0xAA)
    {
        const auto& specialChar = specialCharactersOoT.find(u8Chars.at(index+1));
        if (specialChar != specialCharactersOoT.end())
        {
            codeSize = 2;
            return specialChar->second;
        }
    }

    // Codes
    const auto& macroData = formatCodeMacrosAsciiOoT.find(code);
    if (macroData != formatCodeMacrosAsciiOoT.end())
    {
        size_t macroParams = macroData->second.second;
        codeSize += macroParams;
        return MakeMacroWithArguments(index+1, *macroData);
    }

    codeSize = 0;
    return "";
}


std::string ZMessage::MakeMacroWithArguments(size_t u8Index, const std::pair<uint16_t, std::pair<const char*, size_t>>& macroData)
{
    uint16_t code = macroData.first;
    const char* macroName = macroData.second.first;
    size_t params = macroData.second.second;
    if (params == 0)
    {
        return macroName;
    }

    u8Index += GetMacroArgumentsPadding(code, encoding);

    std::string result = macroName;
    result += "(";
    for (size_t i = 0; i < params; i++)
    {
        if (i != 0)
        {
            result += ", ";
        }

        if (IsCodeTextColor(code, encoding))
        {
            result += colorMacrosOoT.at(u8Chars.at(u8Index + i) & 0x07);
        }
        else
        {
            result += StringHelper::Sprintf("\"\\x%02X\"", u8Chars.at(u8Index + i));
        }
    }
    result += ")";
    return result;
}

size_t ZMessage::GetMacroArgumentsPadding(uint16_t code, ZMessageEncoding encoding)
{
    if (encoding == ZMessageEncoding::Jpn)
    {
        switch (code)
        {
        case 0x000B:
        case 0x86C7:
        case 0x81A3:
        case 0x819E:
        case 0x819A:
        case 0x86B3:
        case 0x86C9:
        case 0x869F:
            return 1;
        }
    }
    return 0;
}

size_t ZMessage::GetBytesPerCode(uint16_t code, ZMessageEncoding encoding)
{
    size_t macroParams = GetMacroArgumentsPadding(code, encoding);

    switch (encoding)
    {
    case ZMessageEncoding::Cn:
        if (code >= 0xA0 && code <= 0xA7)
        {
            return 2;
        }
        // Case fall-through is inteded.
    case ZMessageEncoding::Ascii:
        {
            const auto& macroData = formatCodeMacrosAsciiOoT.find(code);
            if (macroData != formatCodeMacrosAsciiOoT.end())
            {
                macroParams += macroData->second.second;
            }
        }
        return 1 + macroParams;

    case ZMessageEncoding::Jpn:
        {
            const auto& macroData = formatCodeMacrosJpnOoT.find(code);
            if (macroData != formatCodeMacrosJpnOoT.end())
            {
                macroParams += macroData->second.second;
            }
        }
        return 2 + macroParams;
    }

    return 0;
}

bool ZMessage::IsLineBreak(size_t index)
{
    switch (encoding)
    {
    case ZMessageEncoding::Ascii:
    case ZMessageEncoding::Cn:
        return IsCodeLineBreak(u8Chars.at(index), encoding);
    case ZMessageEncoding::Jpn:
        return IsCodeLineBreak(u16Chars.at(index), encoding);
    default:
        return false;
    }
}

bool ZMessage::IsEndMarker(size_t index)
{
    switch (encoding)
    {
    case ZMessageEncoding::Ascii:
    case ZMessageEncoding::Cn:
        return IsCodeEndMarker(u8Chars.at(index), encoding);
    case ZMessageEncoding::Jpn:
        return IsCodeEndMarker(u16Chars.at(index), encoding);
    default:
        return false;
    }
}

bool ZMessage::IsCodeLineBreak(uint16_t code, ZMessageEncoding encoding)
{
    switch (encoding)
    {
    case ZMessageEncoding::Ascii:
    case ZMessageEncoding::Cn:
        if (Globals::Instance->game == ZGame::MM_RETAIL)
        {
            return code == 0x11;
        }
        // OoT // TODO: check if this works for SW97
        return code == 0x01;
    case ZMessageEncoding::Jpn:
        if (Globals::Instance->game == ZGame::MM_RETAIL)
        {
            return code == 0x000A;
        }
        // OoT
        return code == 0x000A;
    default:
        return false;
    }
}

bool ZMessage::IsCodeEndMarker(uint16_t code, ZMessageEncoding encoding)
{
    switch (encoding)
    {
    case ZMessageEncoding::Ascii:
    case ZMessageEncoding::Cn:
        if (Globals::Instance->game == ZGame::MM_RETAIL)
        {
            return code == 0xBF;
        }
        // OoT // TODO: check if this works for SW97
        return code == 0x02;
    case ZMessageEncoding::Jpn:
        if (Globals::Instance->game == ZGame::MM_RETAIL)
        {
            return code == 0x0500;
        }
        // OoT
        return code == 0x8170;
    default:
        return false;
    }
}

bool ZMessage::IsCodeTextColor(uint16_t code, ZMessageEncoding encoding)
{
    switch (encoding)
    {
    case ZMessageEncoding::Ascii:
    case ZMessageEncoding::Cn:
        /*if (Globals::Instance->game == ZGame::MM_RETAIL)
        {
            return code == ;
        }*/
        // OoT // TODO: check if this works for SW97
        return code == 0x05;
    case ZMessageEncoding::Jpn:
        /*if (Globals::Instance->game == ZGame::MM_RETAIL)
        {
            return code == ;
        }*/
        // OoT
        return code == 0x000B;
    default:
        return false;
    }
}

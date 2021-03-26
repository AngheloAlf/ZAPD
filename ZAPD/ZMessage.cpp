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

	name = GetDefaultName(prefix.c_str(), rawDataIndex);
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
    while (true)
    {
        uint16_t code = rawData.at(rawDataIndex + i);
        if (encoding == ZMessageEncoding::Jpn)
        {
            code = BitConverter::ToUInt16BE(rawData, rawDataIndex + i);
        }

        i += GetBytesPerCode(code, encoding);
        if (IsCodeEndMarker(code, encoding))
        {
            break;
        }
    }

    const auto& dataPos = rawData.begin() + rawDataIndex;
    u8Chars.assign(dataPos, dataPos + i);

    // padding
    while ((rawDataIndex + i) % 4 != 0) {
        size_t codeSize = 1;
        uint16_t code = rawData.at(rawDataIndex + i);
        if (encoding == ZMessageEncoding::Jpn)
        {
            codeSize = 2;
            code = BitConverter::ToUInt16BE(rawData, rawDataIndex + i);
        }

        if (code != 0)
        {
            break;
        }

        padding += codeSize;
        i += codeSize;
    }

    for (size_t j = 0; j <  u8Chars.size(); j+=2)
    {
        u16Chars.push_back(BitConverter::ToUInt16BE(u8Chars, j));
    }
}

ZMessage* ZMessage::ExtractFromXML(tinyxml2::XMLElement* reader,
            const std::vector<uint8_t>& nRawData, int nRawDataIndex,
            std::string nRelPath, ZFile* nParent)
{
	ZMessage* message = new ZMessage(reader, nRawData, nRawDataIndex, nParent);
	message->relativePath = std::move(nRelPath);

    message->DeclareVar("", "");

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

void ZMessage::DeclareVar(const std::string& prefix, const std::string& bodyStr)
{
    std::string auxName = name;
    if (name == "")
    {
        auxName = GetDefaultName(prefix, rawDataIndex);
    }
    parent->AddDeclarationArray(rawDataIndex, DeclarationAlignment::Align4, GetRawDataSize(),
                            GetSourceTypeName(), auxName, GetRawDataSize(), bodyStr);
}

std::string ZMessage::GetBodySourceCode()
{
	std::string bodyStr = "    ";
    bool lastWasMacro = true;

    size_t i = 0;
    while (i < GetMessageLength())
    {
        size_t codeSize = 0;
        std::string macro = GetMacro(i, codeSize);

        if (codeSize != 0) // It's a macro
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
                    bodyStr += "\n    ";
                }

                bodyStr += "\"";
                lastWasMacro = false;
            }
            bodyStr += GetCharacterAt(i, codeSize);
            assert(codeSize != 0);
        }

        i += codeSize;
    }

    if (!lastWasMacro)
    {
        bodyStr += "\"";
    }

    return bodyStr;
}

std::string ZMessage::GetSourceOutputCode(const std::string& prefix)
{
    std::string bodyStr = GetBodySourceCode();

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

std::string ZMessage::GetDefaultName(const std::string& prefix, uint32_t address)
{
    return StringHelper::Sprintf("%sMsg_%06X", prefix.c_str(), address);
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
    codeSize = 1;

    if (encoding == ZMessageEncoding::Ascii || encoding == ZMessageEncoding::Cn)
    {
        uint8_t code = u8Chars.at(index);

        if (code == 0x00)
        {
            result += "\\0";
        }
        else if (code == '\"')
        {
            result += "\\\"";
        }
        else if (encoding == ZMessageEncoding::Cn && code >= 0xA0 && code <= 0xA7)
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

    if (encoding == ZMessageEncoding::Jpn)
    {
        uint16_t code = u16Chars.at(index);

        if (code == 0x0000)
        {
            result += "\\0\\0";
        }
        /*else if (code == 0x835C)
        {
            // For some reason, the compiler will not omit the 0x53 part of this character.
            // So now it is a special case
            result += "\\x83\\x5C"; // ソ
        }*/
        else
        {
            result += SHORT_UPPERHALF(code);
            result += SHORT_LOWERHALF(code);
        }

        return result;
    }

    codeSize = 0;
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

    if (Globals::Instance->game == ZGame::MM_RETAIL)
    {
        // Special characters
        const auto& specialChar = specialCharactersAsciiMM.find(code);
        if (specialChar != specialCharactersAsciiMM.end())
        {
            return specialChar->second;
        }

        // Codes
        const auto& macroData = formatCodeMacrosAsciiMM.find(code);
        if (macroData != formatCodeMacrosAsciiMM.end())
        {
            size_t macroParams = macroData->second.second;
            codeSize += macroParams;
            return MakeMacroWithArguments(index+1, *macroData);
        }
    }
    else
    {
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
    }

    codeSize = 0;
    return "";
}

std::string ZMessage::GetJpnMacro(size_t index, size_t& codeSize)
{
    assert(encoding == ZMessageEncoding::Jpn);

    codeSize = 1;
    uint16_t code = u16Chars.at(index);
    uint16_t upperHalf = SHORT_UPPERHALF(code);

    // Special characters
    if (upperHalf == 0x83)
    {
        const auto& specialChar = specialCharactersOoT.find(SHORT_LOWERHALF(code));
        if (specialChar != specialCharactersOoT.end())
        {
            return specialChar->second;
        }
    }

    // Codes
    if (Globals::Instance->game == ZGame::MM_RETAIL)
    {
        const auto& macroData = formatCodeMacrosJpnMM.find(code);
        if (macroData != formatCodeMacrosJpnMM.end())
        {
            size_t macroParams = macroData->second.second;
            // Params amount is in bytes, but we want halfs (or 2bytes).
            codeSize += (macroParams+1) / 2;
        return MakeMacroWithArguments(2 * (index + 1), *macroData);
        }
    }
    else
    {
        const auto& macroData = formatCodeMacrosJpnOoT.find(code);
        if (macroData != formatCodeMacrosJpnOoT.end())
        {
            size_t macroParams = macroData->second.second;
            // Params amount is in bytes, but we want halfs (or 2bytes).
            codeSize += (macroParams+1) / 2;
            return MakeMacroWithArguments(2 * (index + 1), *macroData);
        }
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
    if (Globals::Instance->game == ZGame::MM_RETAIL)
    {
        const auto& macroData = formatCodeMacrosAsciiMM.find(code);
        if (macroData != formatCodeMacrosAsciiMM.end())
        {
            size_t macroParams = macroData->second.second;
            codeSize += macroParams;
            return MakeMacroWithArguments(index+1, *macroData);
        }
    }
    else
    {
        const auto& macroData = formatCodeMacrosAsciiOoT.find(code);
        if (macroData != formatCodeMacrosAsciiOoT.end())
        {
            size_t macroParams = macroData->second.second;
            codeSize += macroParams;
            return MakeMacroWithArguments(index+1, *macroData);
        }
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
        if (Globals::Instance->game == ZGame::MM_RETAIL)
        {
            switch (code)
            {
            case 0x001F: // MSGCODE_INDENT
                return 1;
            }

        }
        else
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
    }
    return 0;
}

int ZMessage::GetBytesPerCode(uint16_t code, ZMessageEncoding encoding)
{
    int macroParams = GetMacroArgumentsPadding(code, encoding);

    switch (encoding)
    {
    case ZMessageEncoding::Cn:
        if (code >= 0xA0 && code <= 0xA7)
        {
            return 2;
        }
        // Case fall-through is inteded.
    case ZMessageEncoding::Ascii:
        if (Globals::Instance->game == ZGame::MM_RETAIL)
        {
            const auto& macroData = formatCodeMacrosAsciiMM.find(code);
            if (macroData != formatCodeMacrosAsciiMM.end())
            {
                macroParams += macroData->second.second;
            }
        }
        else
        {
            const auto& macroData = formatCodeMacrosAsciiOoT.find(code);
            if (macroData != formatCodeMacrosAsciiOoT.end())
            {
                macroParams += macroData->second.second;
            }
        }
        return 1 + macroParams;

    case ZMessageEncoding::Jpn:
        if (Globals::Instance->game == ZGame::MM_RETAIL)
        {
            const auto& macroData = formatCodeMacrosJpnMM.find(code);
            if (macroData != formatCodeMacrosJpnMM.end())
            {
                macroParams += macroData->second.second;
            }
        }
        else
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
    if (Globals::Instance->game == ZGame::MM_RETAIL)
    {
        return false;
    }
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

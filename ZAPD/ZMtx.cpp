#include "ZMtx.h"
#include "BitConverter.h"
#include "StringHelper.h"
#include "ZFile.h"

REGISTER_ZFILENODE(Mtx, ZMtx);

ZMtx::ZMtx(ZFile* nParent) : ZResource(nParent)
{
}

ZMtx::ZMtx(const std::string& prefix, const std::vector<uint8_t>& nRawData, uint32_t nRawDataIndex,
           ZFile* nParent)
	: ZResource(nParent)
{
	name = GetDefaultName(prefix.c_str(), rawDataIndex);
	ExtractFromFile(nRawData, nRawDataIndex);
	DeclareVar("", "");
}

void ZMtx::ParseRawData()
{
	ZResource::ParseRawData();

	uint32_t ptr = rawDataIndex;
	for (size_t i = 0; i < 4; ++i)
	{
		for (size_t j = 0; j < 4; ++j)
		{
			intPart[i][j] = BitConverter::ToUInt16BE(rawData, ptr);
			ptr += 2;
		}
	}
	for (size_t i = 0; i < 4; ++i)
	{
		for (size_t j = 0; j < 4; ++j)
		{
			fracPart[i][j] = BitConverter::ToUInt16BE(rawData, ptr);
			ptr += 2;
		}
	}
}

void ZMtx::ExtractFromXML(tinyxml2::XMLElement* reader, const std::vector<uint8_t>& nRawData,
                          uint32_t nRawDataIndex)
{
	ZResource::ExtractFromXML(reader, nRawData, nRawDataIndex);
	DeclareVar("", "");
}

size_t ZMtx::GetRawDataSize() const
{
	return 64;
}

void ZMtx::DeclareVar(const std::string& prefix, const std::string& bodyStr) const
{
	std::string auxName = name;

	if (name == "")
		auxName = GetDefaultName(prefix, rawDataIndex);

	parent->AddDeclaration(rawDataIndex, DeclarationAlignment::Align8, GetRawDataSize(),
	                       GetSourceTypeName(), auxName, bodyStr);
}

std::string ZMtx::GetBodySourceCode()
{
	std::string bodyStr = "\n";

	for (const auto& row : intPart)
	{
		bodyStr += "\t";

		for (int32_t val : row)
			bodyStr += StringHelper::Sprintf("%6i, ", val);

		bodyStr += "\n";
	}
	for (const auto& row : fracPart)
	{
		bodyStr += "\t";

		for (int32_t val : row)
			bodyStr += StringHelper::Sprintf("%6i, ", val);

		bodyStr += "\n";
	}

	return bodyStr;
}

std::string ZMtx::GetSourceOutputCode(const std::string& prefix)
{
	std::string bodyStr = GetBodySourceCode();

	Declaration* decl = parent->GetDeclaration(rawDataIndex);

	if (decl == nullptr)
		DeclareVar(prefix, bodyStr);
	else
		decl->text = bodyStr;

	return "";
}

std::string ZMtx::GetDefaultName(const std::string& prefix, uint32_t address)
{
	return StringHelper::Sprintf("%sMtx_%06X", prefix.c_str(), address);
}

std::string ZMtx::GetSourceTypeName() const
{
	return "MatrixInternal";
}

ZResourceType ZMtx::GetResourceType() const
{
	return ZResourceType::Mtx;
}

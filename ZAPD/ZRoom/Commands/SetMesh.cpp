#include "SetMesh.h"
#include <Globals.h>
#include <Path.h>
#include "../../BitConverter.h"
#include "../../StringHelper.h"
#include "../../ZFile.h"
#include "../ZRoom.h"
#include "ZBackground.h"

using namespace std;

void GenDListDeclarations(ZRoom* zRoom, ZFile* parent, ZDisplayList* dList);


SetMesh::SetMesh(ZRoom* nZRoom, const std::vector<uint8_t>& nRawData, int nRawDataIndex,
                 int segAddressOffset)
	: ZRoomCommand(nZRoom, nRawData, nRawDataIndex)
{
	string declaration = "";
	meshHeaderType = rawData.at(segmentOffset + 0);

	if (meshHeaderType == 0)
	{
		uint32_t dListStart = Seg2Filespace(BitConverter::ToInt32BE(rawData, segmentOffset + 4),
		                                    parent->baseAddress);
		uint32_t dListEnd = Seg2Filespace(BitConverter::ToInt32BE(rawData, segmentOffset + 8),
		                                  parent->baseAddress);

		int8_t numEntries = rawData[segmentOffset + 1];
		uint32_t currentPtr = dListStart;

		// Hack for Syotes
		for (int i = 0; i < abs(segAddressOffset); i++)
		{
			rawData.erase(rawData.begin());
			segmentOffset--;
		}

		if (numEntries > 0)
		{
			std::string polyGfxBody = "";
			std::string polyGfxType = "";
			int polyGfxSize = 0;

			for (int i = 0; i < numEntries; i++)
			{
				PolygonDlist polyGfxList(zRoom->GetName(), rawData, currentPtr, parent,
				                         zRoom);
				polyGfxBody += polyGfxList.GetBodySourceCode(true);
				polyGfxType = polyGfxList.GetSourceTypeName();
				polyGfxSize = polyGfxList.GetRawDataSize();

				currentPtr += polyGfxList.GetRawDataSize();
			}

			parent->AddDeclarationArray(
				dListStart, DeclarationAlignment::Align4, numEntries * polyGfxSize, polyGfxType,
				StringHelper::Sprintf("%s%s0x%06X", zRoom->GetName().c_str(), polyGfxType.c_str(),
			                          dListStart),
				numEntries, polyGfxBody);
		}

		declaration = "\n    ";
		declaration += StringHelper::Sprintf("{ 0 }, 0x%02X, ", numEntries);
		declaration += "\n    ";

		std::string entriesStr = "NULL";

		if (dListStart != 0)
		{
			entriesStr = parent->GetDeclaration(dListStart)->varName;
		}
		declaration += StringHelper::Sprintf("%s, ", entriesStr.c_str());
		declaration += "\n    ";

		if (dListEnd != 0)
			declaration += StringHelper::Sprintf("%s + ARRAY_COUNT(%s), ", entriesStr.c_str(),
			                                     entriesStr.c_str());
		else
			declaration += "NULL, ";
		declaration += "\n";

		parent->AddDeclaration(
			segmentOffset, DeclarationAlignment::Align16, 12, "MeshHeader0",
			StringHelper::Sprintf("%sMeshHeader0x%06X", zRoom->GetName().c_str(), segmentOffset),
			declaration);

		parent->AddDeclaration(dListStart + (numEntries * 8), DeclarationAlignment::None,
		                              DeclarationPadding::Pad16, 4, "static s32", "terminatorMaybe",
		                              "0x01000000");
	}
	else if (meshHeaderType == 1)
	{
		PolygonType1 polygon1(zRoom->GetName().c_str(), rawData, segmentOffset, parent,
		                      zRoom);
		polygon1.DeclareAndGenerateOutputCode();
	}
	else if (meshHeaderType == 2)
	{
		PolygonType2 polygon2(rawData, segmentOffset, parent, zRoom);
		polygon2.DeclareReferences(zRoom->GetName());

		parent->AddDeclaration(
			segmentOffset, DeclarationAlignment::Align4, polygon2.GetRawDataSize(), polygon2.GetSourceTypeName(),
			StringHelper::Sprintf("%s%s_%06X", zRoom->GetName().c_str(), polygon2.GetSourceTypeName().c_str(), segmentOffset),
			polygon2.GetBodySourceCode());
	}
}

void GenDListDeclarations(ZRoom* zRoom, ZFile* parent, ZDisplayList* dList)
{
	if (dList == nullptr)
	{
		return;
	}
	string srcVarName =
		StringHelper::Sprintf("%s%s", zRoom->GetName().c_str(), dList->GetName().c_str());

	dList->SetName(srcVarName);
	dList->scene = zRoom->scene;
	string sourceOutput = dList->GetSourceOutputCode(zRoom->GetName());

	for (ZDisplayList* otherDList : dList->otherDLists)
		GenDListDeclarations(zRoom, parent, otherDList);

	for (const auto& vtxEntry : dList->vtxDeclarations)
	{
		DeclarationAlignment alignment = DeclarationAlignment::Align4;
		if (Globals::Instance->game == ZGame::MM_RETAIL)
			alignment = DeclarationAlignment::None;
		parent->AddDeclarationArray(
			vtxEntry.first, alignment, dList->vertices[vtxEntry.first].size() * 16, "static Vtx",
			StringHelper::Sprintf("%sVtx_%06X", zRoom->GetName().c_str(), vtxEntry.first),
			dList->vertices[vtxEntry.first].size(), vtxEntry.second);
	}

	for (const auto& texEntry : dList->texDeclarations)
	{
		zRoom->textures[texEntry.first] = dList->textures[texEntry.first];
	}
}

std::string SetMesh::GenDListExterns(ZDisplayList* dList)
{
	string sourceOutput = "";

	if (Globals::Instance->includeFilePrefix)
		sourceOutput += StringHelper::Sprintf("extern Gfx %sDL_%06X[];\n", zRoom->GetName().c_str(),
		                                      dList->GetRawDataIndex());
	else
		sourceOutput += StringHelper::Sprintf("extern Gfx DL_%06X[];\n", dList->GetRawDataIndex());

	for (ZDisplayList* otherDList : dList->otherDLists)
		sourceOutput += GenDListExterns(otherDList);

	for (pair<uint32_t, string> texEntry : dList->texDeclarations)
		sourceOutput += StringHelper::Sprintf("extern u64 %sTex_%06X[];\n",
		                                      zRoom->GetName().c_str(), texEntry.first);

	sourceOutput += dList->defines;

	return sourceOutput;
}

std::string SetMesh::GetBodySourceCode()
{
	Declaration* decl = parent->GetDeclaration(segmentOffset);
	return StringHelper::Sprintf("%s, %i, &%s", GetCommandHex().c_str(), cmdArg1, decl->varName.c_str());
}

string SetMesh::GenerateExterns()
{
	return "";
}

int32_t SetMesh::GetRawDataSize()
{
	return ZRoomCommand::GetRawDataSize();
}

string SetMesh::GetCommandCName()
{
	return "SCmdMesh";
}

RoomCommand SetMesh::GetRoomCommand()
{
	return RoomCommand::SetMesh;
}

PolygonDlist::PolygonDlist(const std::string& prefix, const std::vector<uint8_t>& nRawData,
                           int nRawDataIndex, ZFile* nParent, ZRoom* nRoom)
{
	rawData.assign(nRawData.begin(), nRawData.end());
	rawDataIndex = nRawDataIndex;
	parent = nParent;
	room = nRoom;

	name = GetDefaultName(prefix.c_str(), rawDataIndex);

	ParseRawData();

	opaDList = MakeDlist(opa, prefix);
	xluDList = MakeDlist(xlu, prefix);
}

void PolygonDlist::ParseRawData()
{
	opa = BitConverter::ToUInt32BE(rawData, rawDataIndex);
	xlu = BitConverter::ToUInt32BE(rawData, rawDataIndex + 4);
}

ZDisplayList* PolygonDlist::MakeDlist(segptr_t ptr, const std::string& prefix)
{
	if (ptr == 0)
	{
		return nullptr;
	}

	uint32_t dlistAddress = Seg2Filespace(ptr, parent->baseAddress);

	int dlistLength = ZDisplayList::GetDListLength(
		rawData, dlistAddress,
		Globals::Instance->game == ZGame::OOT_SW97 ? DListType::F3DEX : DListType::F3DZEX);
	ZDisplayList* dlist = new ZDisplayList(rawData, dlistAddress, dlistLength, parent);
	GenDListDeclarations(room, parent, dlist);

	return dlist;
}

int PolygonDlist::GetRawDataSize()
{
	return 0x08;
}

void PolygonDlist::DeclareVar(const std::string& prefix, const std::string& bodyStr)
{
	std::string auxName = name;
	if (name == "")
	{
		auxName = GetDefaultName(prefix, rawDataIndex);
	}
	parent->AddDeclaration(rawDataIndex, DeclarationAlignment::Align4, GetRawDataSize(),
	                       GetSourceTypeName(), auxName, bodyStr);
}

std::string PolygonDlist::GetBodySourceCode(bool arrayElement)
{
	std::string bodyStr = "";
	std::string opaStr = "NULL";
	std::string xluStr = "NULL";
	if (arrayElement)
	{
		bodyStr += "    { \n    ";
	}
	else
	{
		bodyStr += "\n";
	}

	if (opa != 0)
	{
		Declaration* decl = parent->GetDeclaration(Seg2Filespace(opa, parent->baseAddress));
		if (decl != nullptr)
		{
			opaStr = decl->varName;
		}
		else
		{
			opaStr = StringHelper::Sprintf("0x%08X", opa);
		}
	}
	if (xlu != 0)
	{
		Declaration* decl = parent->GetDeclaration(Seg2Filespace(xlu, parent->baseAddress));
		if (decl != nullptr)
		{
			xluStr = decl->varName;
		}
		else
		{
			xluStr = StringHelper::Sprintf("0x%08X", xlu);
		}
	}

	bodyStr += StringHelper::Sprintf("    %s, \n", opaStr.c_str());
	if (arrayElement)
	{
		bodyStr += "    ";
	}

	bodyStr += StringHelper::Sprintf("    %s, \n", xluStr.c_str());

	if (arrayElement)
	{
		bodyStr += "    },";
	}

	return bodyStr;
}

void PolygonDlist::DeclareAndGenerateOutputCode()
{
	std::string bodyStr = GetBodySourceCode(false);

	Declaration* decl = parent->GetDeclaration(rawDataIndex);
	if (decl == nullptr)
	{
		DeclareVar("", bodyStr);
	}
	else
	{
		decl->text = bodyStr;
	}
}

std::string PolygonDlist::GetDefaultName(const std::string& prefix, uint32_t address)
{
	return StringHelper::Sprintf("%sPolyDlist_%06X", prefix.c_str(), address);
}

std::string PolygonDlist::GetSourceTypeName()
{
	return "PolygonDlist";
}

std::string PolygonDlist::GetName()
{
	return name;
}

BgImage::BgImage(bool nIsSubStruct, const std::string& prefix, const std::vector<uint8_t>& nRawData,
                 int nRawDataIndex, ZFile* nParent)
{
	rawData.assign(nRawData.begin(), nRawData.end());
	rawDataIndex = nRawDataIndex;
	parent = nParent;
	isSubStruct = nIsSubStruct;

	name = GetDefaultName(prefix.c_str(), rawDataIndex);

	ParseRawData();
	sourceBackground = MakeBackground(source, prefix);
}

void BgImage::ParseRawData()
{
	size_t pad = 0x00;
	if (!isSubStruct)
	{
		pad = 0x04;

		unk_00 = BitConverter::ToUInt16BE(rawData, rawDataIndex + 0x00);
		id = BitConverter::ToUInt8BE(rawData, rawDataIndex + 0x02);
	}
	source = BitConverter::ToUInt32BE(rawData, rawDataIndex + pad + 0x00);
	unk_0C = BitConverter::ToUInt32BE(rawData, rawDataIndex + pad + 0x04);
	tlut = BitConverter::ToUInt32BE(rawData, rawDataIndex + pad + 0x08);
	width = BitConverter::ToUInt16BE(rawData, rawDataIndex + pad + 0x0C);
	height = BitConverter::ToUInt16BE(rawData, rawDataIndex + pad + 0x0E);
	fmt = BitConverter::ToUInt8BE(rawData, rawDataIndex + pad + 0x10);
	siz = BitConverter::ToUInt8BE(rawData, rawDataIndex + pad + 0x11);
	mode0 = BitConverter::ToUInt16BE(rawData, rawDataIndex + pad + 0x12);
	tlutCount = BitConverter::ToUInt16BE(rawData, rawDataIndex + pad + 0x14);
}

ZBackground* BgImage::MakeBackground(segptr_t ptr, const std::string& prefix)
{
	if (ptr == 0)
	{
		return nullptr;
	}

	uint32_t backAddress = Seg2Filespace(ptr, parent->baseAddress);

	ZBackground* background = new ZBackground(prefix, rawData, backAddress, parent);
	background->DeclareVar(prefix, "");
	parent->resources.push_back(background);

	return background;
}

int BgImage::GetRawDataSize()
{
	return 0x1C;
}

std::string BgImage::GetBodySourceCode(bool arrayElement)
{
	std::string bodyStr = "";
	if (arrayElement)
	{
		bodyStr += "    { \n        ";
	}

	if (!isSubStruct)
	{
		bodyStr += StringHelper::Sprintf("0x%04X, ", unk_00);
		bodyStr += StringHelper::Sprintf("%i, ", id);
		bodyStr += "\n    ";
		if (arrayElement)
		{
			bodyStr += "    ";
		}
	}

	std::string backgroundName = "NULL";
	if (source != 0)
	{
		uint32_t address = Seg2Filespace(source, parent->baseAddress);
		Declaration* decl = parent->GetDeclaration(address);

		if (decl == nullptr)
		{
			backgroundName += StringHelper::Sprintf("0x%08X, ", source);
		}
		else
		{
			backgroundName = decl->varName;
		}
	}
	bodyStr += StringHelper::Sprintf("%s, ", backgroundName.c_str());
	bodyStr += "\n    ";
	if (arrayElement)
	{
		bodyStr += "    ";
	}

	bodyStr += StringHelper::Sprintf("0x%08X, ", unk_0C);
	bodyStr += StringHelper::Sprintf("0x%08X, ", tlut);
	bodyStr += "\n    ";
	if (arrayElement)
	{
		bodyStr += "    ";
	}

	bodyStr += StringHelper::Sprintf("%i, ", width);
	bodyStr += StringHelper::Sprintf("%i, ", height);
	bodyStr += "\n    ";
	if (arrayElement)
	{
		bodyStr += "    ";
	}

	bodyStr += StringHelper::Sprintf("%i, ", fmt);
	bodyStr += StringHelper::Sprintf("%i, ", siz);
	bodyStr += "\n    ";
	if (arrayElement)
	{
		bodyStr += "    ";
	}

	bodyStr += StringHelper::Sprintf("0x%04X, ", mode0);
	bodyStr += StringHelper::Sprintf("0x%04X, ", tlutCount);
	if (arrayElement)
	{
		bodyStr += " \n    }, ";
	}

	return bodyStr;
}

std::string BgImage::GetDefaultName(const std::string& prefix, uint32_t address)
{
	return StringHelper::Sprintf("%sBgImage_%06X", prefix.c_str(), address);
}

std::string BgImage::GetSourceTypeName()
{
	return "BgImage";
}

std::string BgImage::GetName()
{
	return name;
}

PolygonType1::PolygonType1(const std::string& prefix, const std::vector<uint8_t>& nRawData,
                           int nRawDataIndex, ZFile* nParent, ZRoom* nRoom)
{
	rawData.assign(nRawData.begin(), nRawData.end());
	rawDataIndex = nRawDataIndex;
	parent = nParent;

	name = GetDefaultName(prefix.c_str(), rawDataIndex);

	ParseRawData();

	if (dlist != 0)
	{
		polyGfxList =
			PolygonDlist(prefix, rawData, Seg2Filespace(dlist, parent->baseAddress), parent, nRoom);
	}

	uint32_t listAddress;
	std::string bgImageArrayBody = "";
	switch (format)
	{
	case 1:
		single = BgImage(true, prefix, nRawData, nRawDataIndex + 0x08, parent);
		break;

	case 2:
		if (list != 0)
		{
			listAddress = Seg2Filespace(list, parent->baseAddress);
			for (size_t i = 0; i < count; ++i)
			{
				BgImage bg(false, prefix, rawData, listAddress + i * BgImage::GetRawDataSize(),
				           parent);
				multiList.push_back(bg);
				bgImageArrayBody += bg.GetBodySourceCode(true);
				if (i + 1 < count)
				{
					bgImageArrayBody += "\n";
				}
			}

			Declaration* decl = parent->GetDeclaration(listAddress);
			if (decl == nullptr)
			{
				parent->AddDeclarationArray(
					listAddress, DeclarationAlignment::Align4, count * BgImage::GetRawDataSize(),
					BgImage::GetSourceTypeName(), multiList.at(0).GetName().c_str(), count,
					bgImageArrayBody);
			}
		}
		break;

	default:
		throw std::runtime_error(StringHelper::Sprintf(
			"Error in PolygonType1::PolygonType1\n\t Unknown format: %i\n", format));
		break;
	}
}

void PolygonType1::ParseRawData()
{
	type = BitConverter::ToUInt8BE(rawData, rawDataIndex);
	format = BitConverter::ToUInt8BE(rawData, rawDataIndex + 0x01);
	dlist = BitConverter::ToUInt32BE(rawData, rawDataIndex + 0x04);

	if (format == 2)
	{
		count = BitConverter::ToUInt8BE(rawData, rawDataIndex + 0x08);
		list = BitConverter::ToUInt32BE(rawData, rawDataIndex + 0x0C);
	}
}

int PolygonType1::GetRawDataSize()
{
	switch (format)
	{
	case 1:
		return 0x20;

	case 2:
		return 0x10;
	}
	return 0x20;
}

void PolygonType1::DeclareVar(const std::string& prefix, const std::string& bodyStr)
{
	std::string auxName = name;
	if (name == "")
	{
		auxName = GetDefaultName(prefix, rawDataIndex);
	}
	parent->AddDeclaration(rawDataIndex, DeclarationAlignment::Align4, GetRawDataSize(),
	                       GetSourceTypeName(), auxName, bodyStr);
}

std::string PolygonType1::GetBodySourceCode()
{
	std::string bodyStr = "\n    ";

	bodyStr += "{ ";
	bodyStr += StringHelper::Sprintf("%i, %i, ", type, format);

	std::string dlistStr = "NULL";
	if (dlist != 0)
	{
		uint32_t entryRecordAddress = Seg2Filespace(dlist, parent->baseAddress);
		Declaration* decl = parent->GetDeclaration(entryRecordAddress);

		if (decl == nullptr)
		{
			polyGfxList.DeclareAndGenerateOutputCode();
			dlistStr = "&" + polyGfxList.GetName();
		}
		else
		{
			dlistStr = "&" + decl->varName;
		}
	}
	bodyStr += StringHelper::Sprintf("%s, ", dlistStr.c_str());
	bodyStr += "}, \n";

	std::string listStr = "NULL";
	// bodyStr += "    { \n";
	switch (format)
	{
	case 1:
		bodyStr += "    " + single.GetBodySourceCode(false) + "\n";
		break;
	case 2:
		if (list != 0)
		{
			uint32_t listAddress = Seg2Filespace(list, parent->baseAddress);
			Declaration* decl = parent->GetDeclaration(listAddress);
			if (decl != nullptr)
			{
				listStr = decl->varName;
			}
			else
			{
				listStr = StringHelper::Sprintf("0x%08X", list);
			}
		}
		bodyStr += StringHelper::Sprintf("    %i, %s, \n", count, listStr.c_str());
		break;

	default:
		break;
	}
	// bodyStr += "    } \n";

	return bodyStr;
}

void PolygonType1::DeclareAndGenerateOutputCode()
{
	std::string bodyStr = GetBodySourceCode();

	Declaration* decl = parent->GetDeclaration(rawDataIndex);
	if (decl == nullptr)
	{
		DeclareVar("", bodyStr);
	}
	else
	{
		decl->text = bodyStr;
	}
}

std::string PolygonType1::GetDefaultName(const std::string& prefix, uint32_t address)
{
	return StringHelper::Sprintf("%sPolygonType1_%06X", prefix.c_str(), address);
}

std::string PolygonType1::GetSourceTypeName()
{
	switch (format)
	{
	case 1:
		return "MeshHeader1Single";

	case 2:
		return "MeshHeader1Multi";
	}
	return "ERROR";
	// return "PolygonType1";
}

std::string PolygonType1::GetName()
{
	return name;
}

PolygonDlist2::PolygonDlist2(const std::vector<uint8_t>& rawData, int rawDataIndex, ZFile* nParent, ZRoom* nRoom)
	: parent{nParent}
{
	x = BitConverter::ToInt16BE(rawData, rawDataIndex + 0);
	y = BitConverter::ToInt16BE(rawData, rawDataIndex + 2);
	z = BitConverter::ToInt16BE(rawData, rawDataIndex + 4);
	unk_06 = BitConverter::ToInt16BE(rawData, rawDataIndex + 6);

	opa = BitConverter::ToUInt32BE(rawData, rawDataIndex + 8);
	xlu = BitConverter::ToUInt32BE(rawData, rawDataIndex + 12);

	if (opa != 0)
	{
		uint32_t opaOffset = GETSEGOFFSET(opa);
		auto dListLen = ZDisplayList::GetDListLength(rawData, opaOffset, Globals::Instance->game == ZGame::OOT_SW97 ? DListType::F3DEX : DListType::F3DZEX);
		opaDList = new ZDisplayList(rawData, opaOffset, dListLen, parent);
		GenDListDeclarations(nRoom, parent, opaDList);
	}

	if (xlu != 0)
	{
		uint32_t xluOffset = GETSEGOFFSET(xlu);
		auto dListLen = ZDisplayList::GetDListLength(rawData, xluOffset, Globals::Instance->game == ZGame::OOT_SW97 ? DListType::F3DEX : DListType::F3DZEX);
		xluDList = new ZDisplayList(rawData, xluOffset, dListLen, parent);
		GenDListDeclarations(nRoom, parent, xluDList);
	}
}

std::string PolygonDlist2::GetBodySourceCode() const
{
	std::string opaName = "NULL";
	if (opa != 0)
	{
		uint32_t opaOffset = GETSEGOFFSET(opa);
		Declaration* decl = parent->GetDeclaration(opaOffset);
		if (decl != nullptr)
		{
			opaName = decl->varName;
		}
		else
		{
			opaName = StringHelper::Sprintf("0x%08X", opa);
		}
	}

	std::string xluName = "NULL";
	if (xlu != 0)
	{
		uint32_t xluOffset = GETSEGOFFSET(xlu);
		Declaration* decl = parent->GetDeclaration(xluOffset);
		if (decl != nullptr)
		{
			xluName = decl->varName;
		}
		else
		{
			xluName = StringHelper::Sprintf("0x%08X", xluOffset);
		}
	}

	return StringHelper::Sprintf("{ %6i, %6i, %6i }, %6i, %s, %s", x, y, z, unk_06, opaName.c_str(), xluName.c_str());
}

std::string PolygonDlist2::GetSourceTypeName() const
{
	return "PolygonDlist2";
}

int PolygonDlist2::GetRawDataSize() const
{
	return 0x10;
}

PolygonType2::PolygonType2(const std::vector<uint8_t>& nRawData, int nRawDataIndex, ZFile* nParent, ZRoom* nRoom)
	: parent{nParent}
{
	type = BitConverter::ToUInt8BE(nRawData, nRawDataIndex);
	num = BitConverter::ToUInt8BE(nRawData, nRawDataIndex + 0x01);

	start = BitConverter::ToUInt32BE(nRawData, nRawDataIndex + 0x04);
	end = BitConverter::ToUInt32BE(nRawData, nRawDataIndex + 0x08);

	uint32_t currentPtr = GETSEGOFFSET(start);
	for (size_t i = 0; i < num; i++)
	{
		PolygonDlist2 entry(nRawData, currentPtr, parent, nRoom);
		polyDLists.push_back(entry);
		currentPtr += entry.GetRawDataSize();
	}
}

void PolygonType2::DeclareReferences(std::string prefix)
{
	if (num > 0)
	{
		std::string declaration = "";

		for (size_t i = 0; i < polyDLists.size(); i++)
		{
			declaration += StringHelper::Sprintf("    { %s },", polyDLists.at(i).GetBodySourceCode().c_str());
			if (i + 1 < polyDLists.size())
				declaration += "\n";
		}

		std::string polyDlistType = polyDLists.at(0).GetSourceTypeName();
		std::string polyDListName = "";
		polyDListName = StringHelper::Sprintf("%s%s0x%06X", prefix.c_str(), polyDlistType.c_str(), start);

		parent->AddDeclarationArray(
			GETSEGOFFSET(start), DeclarationAlignment::Align4,
			polyDLists.size() * 16, polyDlistType,
			polyDListName,
			polyDLists.size(), declaration);
	}

	parent->AddDeclaration(GETSEGOFFSET(end), DeclarationAlignment::Align4, DeclarationPadding::Pad16, 4, "static s32", "terminatorMaybe", "0x01000000");
}

std::string PolygonType2::GetBodySourceCode()
{
	std::string listName = "NULL";
	if (start != 0)
	{
		uint32_t startOffset = GETSEGOFFSET(start);
		Declaration* decl = parent->GetDeclaration(startOffset);
		if (decl != nullptr)
		{
			listName = "&" + decl->varName;
		}
		else
		{
			listName = StringHelper::Sprintf("0x%08X", startOffset);
		}
	}

	std::string body = StringHelper::Sprintf("\n    %i, %i,\n", type, polyDLists.size());
	body += StringHelper::Sprintf("    %s,\n", listName.c_str());
	body += StringHelper::Sprintf("    %s + ARRAY_COUNTU(%s)\n", listName.c_str(), listName.c_str());
	return body;
}

std::string PolygonType2::GetSourceTypeName() const
{
	return "PolygonType2";
}

int PolygonType2::GetRawDataSize() const
{
	return 0x0C;
}

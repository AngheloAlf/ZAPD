#include "SetExitList.h"
#include "../../BitConverter.h"
#include "../../StringHelper.h"
#include "../../ZFile.h"
#include "../ZRoom.h"

using namespace std;

SetExitList::SetExitList(ZRoom* nZRoom, std::vector<uint8_t> rawData, int rawDataIndex)
	: ZRoomCommand(nZRoom, rawData, rawDataIndex)
{
	if (segmentOffset != 0)
		zRoom->parent->AddDeclarationPlaceholder(segmentOffset);
}

std::string SetExitList::GetBodySourceCode()
{
	return StringHelper::Sprintf("%s, 0x00, (u32)&%sExitList0x%06X", GetCommandHex().c_str(), zRoom->GetName().c_str(), segmentOffset);
}

string SetExitList::GenerateSourceCodePass1(string roomName, int baseAddress)
{
	// Parse Entrances and Generate Declaration
	zRoom->parent->AddDeclarationPlaceholder(segmentOffset);  // Make sure this segment is defined
	int numEntrances = zRoom->GetDeclarationSizeFromNeighbor(segmentOffset) / 2;
	uint32_t currentPtr = segmentOffset;

	for (int i = 0; i < numEntrances; i++)
	{
		uint16_t exit = BitConverter::ToInt16BE(rawData, currentPtr);
		exits.push_back(exit);

		currentPtr += 2;
	}

	string declaration = "";

	for (uint16_t exit : exits)
		declaration += StringHelper::Sprintf("    0x%04X,\n", exit);
	;

	zRoom->parent->AddDeclarationArray(
		segmentOffset, DeclarationAlignment::Align4, exits.size() * 2, "u16",
		StringHelper::Sprintf("%sExitList0x%06X", zRoom->GetName().c_str(), segmentOffset),
		exits.size(), declaration);

	return GetBodySourceCode();
}

string SetExitList::GenerateExterns()
{
	return StringHelper::Sprintf("extern u16 %sExitList0x%06X[];\n", zRoom->GetName().c_str(),
	                             segmentOffset);
	;
}

string SetExitList::GetCommandCName()
{
	return "SCmdExitList";
}

RoomCommand SetExitList::GetRoomCommand()
{
	return RoomCommand::SetExitList;
}

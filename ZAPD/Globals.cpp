#include "Globals.h"
#include <algorithm>
#include "File.h"
#include "Path.h"
#include "tinyxml2.h"

using namespace tinyxml2;

Globals* Globals::Instance;

Globals::Globals()
{
	Instance = this;

	game = ZGame::OOT_RETAIL;
	genSourceFile = true;
	testMode = false;
	profile = false;
	useLegacyZDList = false;
	useExternalResources = true;
	lastScene = nullptr;
	verbosity = VerbosityLevel::VERBOSITY_SILENT;

	// TODO: don't hardcode
	externalXmlFolder = "assets/xml/";
}

Globals::~Globals()
{
	for (auto& declPair : segmentRefFiles)
	{
		for (auto& file : declPair.second)
		{
			delete file;
		}
	}
}

std::string Globals::FindSymbolSegRef(int32_t segNumber, uint32_t symbolAddress)
{
	if (segmentRefs.find(segNumber) != segmentRefs.end())
	{
		if (segmentRefFiles.find(segNumber) == segmentRefFiles.end())
		{
			XMLDocument doc;
			std::string filePath = segmentRefs[segNumber];
			XMLError eResult = doc.LoadFile(filePath.c_str());

			if (eResult != tinyxml2::XML_SUCCESS)
				return "ERROR";

			XMLNode* root = doc.FirstChild();

			if (root == nullptr)
				return "ERROR";

			for (XMLElement* child = root->FirstChildElement(); child != NULL;
			     child = child->NextSiblingElement())
			{
				if (std::string(child->Name()) == "File")
				{
					ZFile* file = new ZFile(fileMode, child, "", "", "", filePath, true);
					file->GeneratePlaceholderDeclarations();
					AddSegment(segNumber, file);
					break;
				}
			}
		}

		for (auto& file : segmentRefFiles[segNumber])
		{
			if (file->HasDeclaration(symbolAddress))
				return file->GetDeclarationPtrName((segNumber << 0x18) | symbolAddress);
		}
	}

	return "ERROR";
}

void Globals::ReadConfigFile(const std::string& configFilePath)
{
	XMLDocument doc;
	XMLError eResult = doc.LoadFile(configFilePath.c_str());

	if (eResult != tinyxml2::XML_SUCCESS)
	{
		throw std::runtime_error("Error: Unable to read config file.");
		return;
	}

	XMLNode* root = doc.FirstChild();

	if (root == nullptr)
		return;

	for (XMLElement* child = root->FirstChildElement(); child != NULL;
	     child = child->NextSiblingElement())
	{
		if (std::string(child->Name()) == "SymbolMap")
		{
			std::string fileName = std::string(child->Attribute("File"));
			GenSymbolMap(Path::GetDirectoryName(configFilePath) + "/" + fileName);
		}
		else if (std::string(child->Name()) == "Segment")
		{
			std::string fileName = std::string(child->Attribute("File"));
			int32_t segNumber = child->IntAttribute("Number");
			segmentRefs[segNumber] = fileName;
		}
		else if (std::string(child->Name()) == "ActorList")
		{
			std::string fileName = std::string(child->Attribute("File"));
			std::vector<std::string> lines =
				File::ReadAllLines(Path::GetDirectoryName(configFilePath) + "/" + fileName);

			for (std::string line : lines)
				cfg.actorList.push_back(StringHelper::Strip(line, "\r"));
		}
		else if (std::string(child->Name()) == "ObjectList")
		{
			std::string fileName = std::string(child->Attribute("File"));
			std::vector<std::string> lines =
				File::ReadAllLines(Path::GetDirectoryName(configFilePath) + "/" + fileName);

			for (std::string line : lines)
				cfg.objectList.push_back(StringHelper::Strip(line, "\r"));
		}
		else if (std::string(child->Name()) == "TexturePool")
		{
			std::string fileName = std::string(child->Attribute("File"));
			ReadTexturePool(Path::GetDirectoryName(configFilePath) + "/" + fileName);
		}
		else if (std::string(child->Name()) == "BGConfig")
		{
			cfg.bgScreenWidth = child->IntAttribute("ScreenWidth", 320);
			cfg.bgScreenHeight = child->IntAttribute("ScreenHeight", 240);
		}
	}
}

void Globals::ReadTexturePool(const std::string& texturePoolXmlPath)
{
	XMLDocument doc;
	XMLError eResult = doc.LoadFile(texturePoolXmlPath.c_str());

	if (eResult != tinyxml2::XML_SUCCESS)
	{
		fprintf(stderr, "Warning: Unable to read texture pool XML with error code %i\n", eResult);
		return;
	}

	XMLNode* root = doc.FirstChild();

	if (root == nullptr)
		return;

	for (XMLElement* child = root->FirstChildElement(); child != NULL;
	     child = child->NextSiblingElement())
	{
		if (std::string(child->Name()) == "Texture")
		{
			std::string crcStr = std::string(child->Attribute("CRC"));
			fs::path texPath = std::string(child->Attribute("Path"));
			std::string texName = "";

			uint32_t crc = strtoul(crcStr.c_str(), NULL, 16);

			cfg.texturePool[crc].path = texPath;
		}
	}
}

void Globals::GenSymbolMap(const std::string& symbolMapPath)
{
	auto symbolLines = File::ReadAllLines(symbolMapPath);

	for (std::string symbolLine : symbolLines)
	{
		auto split = StringHelper::Split(symbolLine, " ");
		uint32_t addr = strtoul(split[0].c_str(), NULL, 16);
		std::string symbolName = split[1];

		symbolMap[addr] = symbolName;
	}
}

void Globals::AddSegment(int32_t segment, ZFile* file)
{
	if (std::find(segments.begin(), segments.end(), segment) == segments.end())
		segments.push_back(segment);
	if (segmentRefFiles.find(segment) == segmentRefFiles.end())
		segmentRefFiles[segment] = std::vector<ZFile*>();

	segmentRefs[segment] = file->GetXmlFilePath();
	segmentRefFiles[segment].push_back(file);
}

bool Globals::HasSegment(int32_t segment)
{
	return std::find(segments.begin(), segments.end(), segment) != segments.end();
}

bool Globals::GetSegmentedPtrName(segptr_t segAddress, ZFile* currentFile, std::string& declName)
{
	if (segAddress == 0)
	{
		declName = "NULL";
		return true;
	}

	uint8_t segment = GETSEGNUM(segAddress);

	if (segment == currentFile->segment)
	{
		uint32_t address = Seg2Filespace(segAddress, currentFile->baseAddress);
		if (currentFile->HasDeclaration(address))
		{
			declName = currentFile->GetDeclarationPtrName(segAddress);
			return true;
		}
	}
	else if (HasSegment(segment))
	{
		for (auto file : segmentRefFiles[segment])
		{
			uint32_t address = Seg2Filespace(segAddress, file->baseAddress);
			if (file->HasDeclaration(address))
			{
				declName = file->GetDeclarationPtrName(segAddress);
				return true;
			}
		}
	}

	const auto& symbolFromMap = Globals::Instance->symbolMap.find(segAddress);
	if (symbolFromMap != Globals::Instance->symbolMap.end())
	{
		declName = "&" + symbolFromMap->second;
		return true;
	}

	declName = StringHelper::Sprintf("0x%08X", segAddress);
	return false;
}

#pragma once
#include <filesystem>
#include <string>
namespace Files
{
bool WriteReplacement(const std::filesystem::path& path,const std::string& bytes,std::wstring& error);
}

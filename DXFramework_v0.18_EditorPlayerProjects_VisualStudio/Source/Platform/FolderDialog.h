#pragma once
#include <Windows.h>
#include <filesystem>
#include <optional>
namespace Native
{
std::optional<std::filesystem::path> ChooseFolder(HWND owner,const wchar_t* title);
std::filesystem::path UserStateDirectory();
}

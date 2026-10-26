#pragma once
#include <Windows.h>
#include <filesystem>
#include <optional>
#include <string>

namespace SceneFileDialogs
{
    std::optional<std::filesystem::path> Choose(HWND owner,bool save,std::wstring& error);
    // Replace an existing file only after its replacement is completely written.
    bool WriteReplacement(const std::filesystem::path& path,const std::string& bytes,std::wstring& error);
}

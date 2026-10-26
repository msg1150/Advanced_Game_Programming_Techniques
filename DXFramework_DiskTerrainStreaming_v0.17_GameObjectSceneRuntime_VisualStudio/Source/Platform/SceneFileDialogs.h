#pragma once
#include <Windows.h>
#include <filesystem>
#include <optional>
#include <string>

namespace SceneFileDialogs
{
    struct Options
    {
        const wchar_t* Filter=L"Collider Scene (*.scene)\0*.scene\0All files\0*.*\0\0";
        const wchar_t* Extension=L"scene";
        const wchar_t* Title=nullptr;
    };
    std::optional<std::filesystem::path> Choose(HWND owner,bool save,std::wstring& error,Options options={});
    // Replace an existing file only after its replacement is completely written.
    bool WriteReplacement(const std::filesystem::path& path,const std::string& bytes,std::wstring& error);
}

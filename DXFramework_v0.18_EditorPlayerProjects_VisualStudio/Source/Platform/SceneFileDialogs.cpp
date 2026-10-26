#include "Platform/SceneFileDialogs.h"
#include "Core/IO/AtomicFile.h"
#include <commdlg.h>
#include <algorithm>
#include <cwchar>
#include <fstream>
#include <vector>

std::optional<std::filesystem::path> SceneFileDialogs::Choose(HWND owner,bool save,std::wstring& error,Options options)
{
    error.clear();std::vector<wchar_t> filename(32768,L'\0');
    if(options.InitialFile)
    {const auto count=std::wcslen(options.InitialFile);if(count<filename.size())std::copy(options.InitialFile,options.InitialFile+count,filename.begin());}
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=owner;
    dialog.lpstrFilter=options.Filter;dialog.lpstrTitle=options.Title;
    dialog.lpstrFile=filename.data();dialog.nMaxFile=static_cast<DWORD>(filename.size());
    dialog.lpstrDefExt=options.Extension;
    dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|
        (save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    if(save?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog))return std::filesystem::path(filename.data());
    if(const DWORD code=CommDlgExtendedError())error=L"File dialog failed: "+std::to_wstring(code);
    return std::nullopt;
}

bool SceneFileDialogs::WriteReplacement(const std::filesystem::path& path,const std::string& bytes,std::wstring& error)
{return Files::WriteReplacement(path,bytes,error);}

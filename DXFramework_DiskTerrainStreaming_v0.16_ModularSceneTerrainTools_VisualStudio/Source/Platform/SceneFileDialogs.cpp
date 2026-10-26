#include "Platform/SceneFileDialogs.h"
#include <commdlg.h>
#include <fstream>
#include <vector>

std::optional<std::filesystem::path> SceneFileDialogs::Choose(HWND owner,bool save,std::wstring& error)
{
    error.clear();std::vector<wchar_t> filename(32768,L'\0');
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=owner;
    dialog.lpstrFilter=L"Collider Scene (*.scene)\0*.scene\0All files\0*.*\0\0";
    dialog.lpstrFile=filename.data();dialog.nMaxFile=static_cast<DWORD>(filename.size());
    dialog.lpstrDefExt=L"scene";
    dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|
        (save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    if(save?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog))return std::filesystem::path(filename.data());
    if(const DWORD code=CommDlgExtendedError())error=L"File dialog failed: "+std::to_wstring(code);
    return std::nullopt;
}

bool SceneFileDialogs::WriteReplacement(const std::filesystem::path& path,const std::string& bytes,std::wstring& error)
{
    error.clear();
    const auto directory=path.has_parent_path()?path.parent_path():std::filesystem::current_path();
    wchar_t temporary[MAX_PATH]{};
    if(!GetTempFileNameW(directory.c_str(),L"dxf",0,temporary))
    {error=L"Cannot create temporary scene file.";return false;}
    bool written=false;
    {
        std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
        out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));out.flush();
        written=static_cast<bool>(out);out.close();written=written&&!out.fail();
    }
    if(!written||!MoveFileExW(temporary,path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
    {
        error=L"Scene save failed; existing destination was preserved.";
        DeleteFileW(temporary);return false;
    }
    return true;
}

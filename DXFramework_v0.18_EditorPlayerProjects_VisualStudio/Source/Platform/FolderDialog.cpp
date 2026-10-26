#include "Platform/FolderDialog.h"
#include "Platform/UnicodeProcess.h"
#include <shobjidl.h>
#include <shlobj.h>
#include <wrl/client.h>
namespace Native
{
std::optional<std::filesystem::path> ChooseFolder(HWND owner,const wchar_t* title)
{
    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(dialog.GetAddressOf()))))return {};
    DWORD flags=0;dialog->GetOptions(&flags);dialog->SetOptions(flags|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM);dialog->SetTitle(title);
    if(FAILED(dialog->Show(owner)))return {};Microsoft::WRL::ComPtr<IShellItem> item;
    if(FAILED(dialog->GetResult(item.GetAddressOf())))return {};PWSTR path=nullptr;
    if(FAILED(item->GetDisplayName(SIGDN_FILESYSPATH,&path)))return {};
    std::filesystem::path result(path);CoTaskMemFree(path);return result;
}
std::filesystem::path UserStateDirectory()
{
    PWSTR path=nullptr;
    if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&path)))
    {std::filesystem::path result=std::filesystem::path(path)/L"DXFrameworkEditor";CoTaskMemFree(path);return result;}
    return ExecutableDirectory()/L"UserData";
}
}

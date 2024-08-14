#include "platformutils.hpp"

#include "syzygy/core/log.hpp"
#include <GLFW/glfw3.h>
#include <ShObjIdl.h>
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace
{
auto getPathsFromDialog(HWND const parent, DWORD const additionalOptions)
    -> std::vector<std::filesystem::path>
{
    std::vector<std::filesystem::path> paths{};
    IFileOpenDialog* fileDialog;
    if (FAILED(CoCreateInstance(
            CLSID_FileOpenDialog,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&fileDialog)
        )))
    {
        return paths;
    }

    DWORD dwOptions;
    if (SUCCEEDED(fileDialog->GetOptions(&dwOptions)))
    {
        fileDialog->SetOptions(dwOptions | additionalOptions);
    }

    if (SUCCEEDED(fileDialog->Show(parent)))
    {
        IShellItemArray* items;
        if (SUCCEEDED(fileDialog->GetResults(&items)))
        {
            IEnumShellItems* itemsEnum;
            if (SUCCEEDED(items->EnumItems(&itemsEnum)))
            {
                do
                {
                    IShellItem* child;
                    ULONG fetched;
                    itemsEnum->Next(1, &child, &fetched);
                    if (fetched == FALSE)
                    {
                        break;
                    }

                    WCHAR* path;
                    child->GetDisplayName(SIGDN_FILESYSPATH, &path);
                    paths.emplace_back(path);
                    CoTaskMemFree(path);
                } while (TRUE);
                itemsEnum->Release();
            }
            items->Release();
        }
    }
    fileDialog->Release();

    return paths;
}
auto openDialog(
    PlatformWindow const& parent, bool const pickFolders, bool const multiselect
) -> std::vector<std::filesystem::path>
{
    HRESULT const initResult{CoInitializeEx(
        nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE
    )};

    std::vector<std::filesystem::path> paths{};
    if (SUCCEEDED(initResult))
    {
        DWORD additionalOptions{FOS_NOCHANGEDIR};
        if (pickFolders)
        {
            additionalOptions |= FOS_PICKFOLDERS;
        }
        if (multiselect)
        {
            additionalOptions |= FOS_ALLOWMULTISELECT;
        }
        paths = getPathsFromDialog(
            glfwGetWin32Window(parent.handle()), additionalOptions
        );
    }

    CoUninitialize();

    return paths;
}
} // namespace

auto szg_utils::openFile(PlatformWindow const& parent)
    -> std::optional<std::filesystem::path>
{
    std::vector<std::filesystem::path> paths{openDialog(parent, false, false)};

    if (paths.empty())
    {
        return std::nullopt;
    }

    if (paths.size() > 1)
    {
        SZG_WARNING("Dialog box returned more than 1 path, ignoring the rest.");
    }

    return paths[0];
}

auto szg_utils::openFiles(PlatformWindow const& parent)
    -> std::vector<std::filesystem::path>
{
    std::vector<std::filesystem::path> paths{openDialog(parent, false, true)};

    return paths;
}

auto szg_utils::openDirectory(PlatformWindow const& parent)
    -> std::optional<std::filesystem::path>
{
    std::vector<std::filesystem::path> paths{openDialog(parent, true, false)};

    if (paths.empty())
    {
        return std::nullopt;
    }

    if (paths.size() > 1)
    {
        SZG_WARNING("Dialog box returned more than 1 path, ignoring the rest.");
    }

    return paths[0];
}

auto szg_utils::openDirectories(PlatformWindow const& parent)
    -> std::vector<std::filesystem::path>
{
    std::vector<std::filesystem::path> paths{openDialog(parent, true, true)};

    return paths;
}

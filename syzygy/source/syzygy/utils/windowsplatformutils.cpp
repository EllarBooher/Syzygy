#include "platformutils.hpp"

#include "syzygy/helpers.hpp"
#include <GLFW/glfw3.h>
#include <ShObjIdl.h>
#include <Windows.h>
#include <combaseapi.h>
#include <commdlg.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <codecvt>
#include <iostream>
#include <locale>

namespace
{
template <class T = IFileOpenDialog> struct ComPtr
{
    static_assert(
        std::is_base_of_v<IUnknown, T>,
        "Pointer type parameter of ComPtr must derive from IUnknown"
    );
    ComPtr() {}
    ComPtr(T* ptr) { m_ptr = ptr; }
    auto operator->() const -> T* { return m_ptr; }

    ComPtr(ComPtr<T> const&) = delete;
    ComPtr(ComPtr<T>&&) = delete;
    auto operator=(ComPtr<T> const&) -> ComPtr<T>& = delete;
    auto operator=(ComPtr<T>&&) -> ComPtr<T>& = delete;

    ~ComPtr()
    {
        if (m_ptr != nullptr)
        {
            m_ptr->Release();
        }
    }

private:
    T* m_ptr{};
};

auto openDialog(
    PlatformWindow const& parent, bool const pickFolders, bool const multiselect
) -> std::vector<std::filesystem::path>
{
    std::vector<std::filesystem::path> paths{};

    // TODO: I am not very familiar with Windows API code. I am not sure if this
    // is okay to call each time here, or if it should be called earlier and
    // only once.
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
    {
        return paths;
    }

    IFileOpenDialog* pFileDialog;
    if (FAILED(CoCreateInstance(
            CLSID_FileOpenDialog,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&pFileDialog)
        )))
    {
        return paths;
    }
    ComPtr<IFileOpenDialog> const fileDialog{pFileDialog};

    DWORD dwOptions;
    if (SUCCEEDED(fileDialog->GetOptions(&dwOptions)))
    {
        if (pickFolders)
        {
            dwOptions |= FOS_PICKFOLDERS;
        }
        if (multiselect)
        {
            dwOptions |= FOS_ALLOWMULTISELECT;
        }

        fileDialog->SetOptions(dwOptions | FOS_NOCHANGEDIR);
    }

    HWND const nativeWindow{glfwGetWin32Window(parent.handle())};
    if (FAILED(fileDialog->Show(nativeWindow)))
    {
        return paths;
    }

    IShellItemArray* pItems;
    if (FAILED(fileDialog->GetResults(&pItems)))
    {
        return paths;
    }
    ComPtr<IShellItemArray> const items{pItems};

    IEnumShellItems* pItemsEnum;
    if (FAILED(items->EnumItems(&pItemsEnum)))
    {
        return paths;
    }
    ComPtr<IEnumShellItems> const itemsEnum{pItemsEnum};

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
    } while (TRUE);

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
        Warning("Dialog box returned more than 1 path, ignoring the rest.");
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
        Warning("Dialog box returned more than 1 path, ignoring the rest.");
    }

    return paths[0];
}

auto szg_utils::openDirectories(PlatformWindow const& parent)
    -> std::vector<std::filesystem::path>
{
    std::vector<std::filesystem::path> paths{openDialog(parent, true, true)};

    return paths;
}

#pragma once
#include <format>
#include <functional>
#include <shobjidl.h>

#include "debug.hpp"

#ifdef NDEBUG
#define SPIDER_DBG_CODE(code)
#define SPIDER_DBG_VAR(var)
#define SPIDER_DX12_ERROR_CHECK_PREPARE
#define SPIDER_DX12_ERROR_CHECK(expr) expr
#define SPIDER_CODE_SWAP(dbg, rel) rel
#else
#define SPIDER_DBG_CODE(code) code
#define SPIDER_DBG_VAR(var) var
#define SPIDER_DX12_ERROR_CHECK_PREPARE ID3D12Device* device_ = nullptr;
#define SPIDER_DX12_ERROR_CHECK(expr)                                        \
{                                                                            \
    HRESULT hr = (expr);                                                     \
    if (hr == DXGI_ERROR_DEVICE_REMOVED) {                                   \
        _com_error err(device_->GetDeviceRemovedReason());                   \
        std::wcout << L"[DX12 ERROR] Device Removed: "                       \
                   << err.ErrorMessage() << std::endl;                       \
        throw std::runtime_error("DX12 Device was removed.");                \
    }                                                                        \
    else if (FAILED(hr)) {                                                   \
        _com_error err(hr);                                                  \
        std::wcout << L"[DX12 ERROR] " << err.ErrorMessage() << std::endl;   \
        throw std::runtime_error("DX12 Error.");                             \
    }                                                                        \
}
#define SPIDER_CODE_SWAP(dbg, rel) dbg
#endif

#define SPIDER_RAW_BITCAST(Target, origin) (*reinterpret_cast<Target*>(reinterpret_cast<void*>(&origin)))

using uint_t = unsigned int;

namespace spider_engine::helpers {
    std::string toString(const std::wstring& wstr) {
        if (wstr.empty()) return {};

        int sizeNeeded = WideCharToMultiByte(
            CP_UTF8, 
            0, 
            wstr.data(),
            (int)wstr.size(), 
            nullptr, 
            0, 
            nullptr, 
            nullptr
        );

        std::string result(sizeNeeded, 0);

        WideCharToMultiByte(
            CP_UTF8, 
            0, 
            wstr.data(),
            (int)wstr.size(), 
            result.data(), 
            sizeNeeded, 
            nullptr, 
            nullptr
        );

        return result;
    }

    std::wstring openFolderDialog() {
        IFileOpenDialog* pFileOpen = nullptr;
        HRESULT hr = CoCreateInstance(
            CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
            IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen)
        );

        if (FAILED(hr)) return L"";

        DWORD options;
        pFileOpen->GetOptions(&options);
        pFileOpen->SetOptions(options | FOS_PICKFOLDERS);

        hr = pFileOpen->Show(nullptr);
        if (FAILED(hr)) return L"";

        IShellItem* pItem = nullptr;
        hr = pFileOpen->GetResult(&pItem);
        if (FAILED(hr)) return L"";

        PWSTR pszFilePath = nullptr;
        hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

        std::wstring path = pszFilePath;
        CoTaskMemFree(pszFilePath);
        pItem->Release();
        pFileOpen->Release();

        return path;
    }

    std::wstring getExecutablePath()
    {
        wchar_t buffer[MAX_PATH];
        GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        return buffer;
    }
}

bool isButtonDown(int vkey) {
    bool prev[256] = {};

    bool down = (GetAsyncKeyState(vkey) & 0x8000) != 0;
    bool pressed = down && !prev[vkey];

    prev[vkey] = down;
    return pressed;
}
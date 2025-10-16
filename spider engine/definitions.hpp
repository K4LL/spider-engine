#include <format>

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
#include <RED4ext/RED4ext.hpp>
#include <RED4ext/Scripting/Natives/Generated/Vector2.hpp>
#include <RED4ext/Scripting/Utils.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace fs = std::filesystem;

namespace {
    const RED4ext::v1::Sdk* sdk = nullptr;
    RED4ext::v1::PluginHandle pluginHandle{};
    std::mutex g_notesMutex;
    std::unordered_map<int32_t, std::string> g_notesCache;

    fs::path GetSavesPath() {
        const char* home = std::getenv("HOME");
        if (!home) return "";
        return fs::path(home) / "Library" / "Application Support" / "CD Projekt Red" / "Cyberpunk 2077" / "saves";
    }

    int32_t GetLastSaveIndex() {
        auto savesDir = GetSavesPath();
        std::error_code ec;
        if (!fs::exists(savesDir, ec)) return -1;

        int32_t newestIndex = -1;
        fs::file_time_type newestTime{};
        bool found = false;

        for (const auto& entry : fs::directory_iterator(savesDir, ec)) {
            if (!entry.is_directory(ec)) continue;
            std::string name = entry.path().filename().string();
            if (name.rfind("ManualSave-", 0) == 0) {
                try {
                    int32_t idx = std::stoi(name.substr(11));
                    auto writeTime = entry.last_write_time(ec);
                    if (!found || writeTime > newestTime) {
                        newestTime = writeTime;
                        newestIndex = idx;
                        found = true;
                    }
                } catch (...) {}
            }
        }
        return newestIndex;
    }

    bool StoreCustomNote(std::string_view note) {
        std::lock_guard<std::mutex> lock(g_notesMutex);
        int32_t idx = GetLastSaveIndex();
        if (idx < 0) return false;

        auto filePath = GetSavesPath() / ("ManualSave-" + std::to_string(idx)) / "NamedSave.txt";
        std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        out.write(note.data(), note.size());
        out.close();

        g_notesCache[idx] = std::string(note);
        return true;
    }

    std::string GetCustomNoteByIndex(int32_t idx) {
        if (idx < 0) return "";
        std::lock_guard<std::mutex> lock(g_notesMutex);
        auto it = g_notesCache.find(idx);
        if (it != g_notesCache.end()) return it->second;

        auto filePath = GetSavesPath() / ("ManualSave-" + std::to_string(idx)) / "NamedSave.txt";
        std::ifstream in(filePath, std::ios::binary);
        if (!in.is_open()) return "";
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();

        g_notesCache[idx] = content;
        return content;
    }
}

void AddCustomNoteToNewestSave(RED4ext::IScriptable* aContext, RED4ext::CStackFrame* aFrame, void* aOut, int64_t a4) noexcept {
    RED4ext::CString note;
    RED4ext::GetParameter(aFrame, &note);
    aFrame->code++;
    StoreCustomNote(std::string_view(note.c_str(), note.Length()));
}

void TryAddCustomNoteToNewestSave(RED4ext::IScriptable* aContext, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t a4) noexcept {
    RED4ext::CString note;
    RED4ext::GetParameter(aFrame, &note);
    aFrame->code++;
    bool res = StoreCustomNote(std::string_view(note.c_str(), note.Length()));
    if (aOut) *aOut = res;
}

void GetNoteForSaveIndex(RED4ext::IScriptable* aContext, RED4ext::CStackFrame* aFrame, RED4ext::CString* aOut, int64_t a4) noexcept {
    if (aOut) *aOut = RED4ext::CString();
    int32_t index = -1;
    RED4ext::GetParameter(aFrame, &index);
    aFrame->code++;
    std::string note = GetCustomNoteByIndex(index);
    if (aOut) *aOut = RED4ext::CString(note.c_str());
}

RED4EXT_C_EXPORT void RED4EXT_CALL RegisterTypes() {}

RED4EXT_C_EXPORT void RED4EXT_CALL PostRegisterTypes() {
    try {
        auto rtti = RED4ext::CRTTISystem::Get();
        RED4ext::CBaseFunction::Flags flags = {.isNative = true, .isStatic = true};
        {
            auto func = RED4ext::CGlobalFunction::Create("AddCustomNoteToNewestSave", "AddCustomNoteToNewestSave", &AddCustomNoteToNewestSave);
            func->flags = flags;
            func->AddParam("String", "note");
            rtti->RegisterFunction(func);
        }
        {
            auto func = RED4ext::CGlobalFunction::Create("TryAddCustomNoteToNewestSave", "TryAddCustomNoteToNewestSave", &TryAddCustomNoteToNewestSave);
            func->flags = flags;
            func->AddParam("String", "note");
            func->SetReturnType("Bool");
            rtti->RegisterFunction(func);
        }
        {
            auto func = RED4ext::CGlobalFunction::Create("GetNoteForSaveIndex", "GetNoteForSaveIndex", &GetNoteForSaveIndex);
            func->flags = flags;
            func->AddParam("Int32", "index");
            func->SetReturnType("String");
            rtti->RegisterFunction(func);
        }
    } catch (...) {}
}

RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::v1::PluginHandle aHandle, RED4ext::v1::EMainReason aReason, const RED4ext::v1::Sdk* aSdk) {
    switch (aReason) {
        case RED4ext::v1::EMainReason::Load: {
            sdk = aSdk;
            pluginHandle = aHandle;
            auto rtti = RED4ext::CRTTISystem::Get();
            rtti->AddRegisterCallback(RegisterTypes);
            rtti->AddPostRegisterCallback(PostRegisterTypes);
            if (sdk && sdk->logger && sdk->logger->Info) {
                sdk->logger->Info(pluginHandle, "NamedSaves (macOS ARM64) loaded successfully");
            }
            break;
        }
        case RED4ext::v1::EMainReason::Unload: {
            sdk = nullptr;
            break;
        }
    }
    return true;
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* aInfo) {
    aInfo->name = L"NamedSaves";
    aInfo->author = L"DJ_Kovrik (macOS port)";
    aInfo->version = RED4EXT_V1_SEMVER(2, 7, 0);
    aInfo->runtime = RED4EXT_V1_RUNTIME_VERSION_INDEPENDENT;
    aInfo->sdk = RED4EXT_V1_SDK_VERSION_1_0_0_COMPAT_0_5_0;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports() {
    return RED4EXT_API_VERSION_1_COMPAT_0;
}

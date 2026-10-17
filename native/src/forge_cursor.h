#pragma once
#include "common.h"
#include <atomic>
#include <cstring>
namespace fx::forge::cursor {
using Function=BOOL(WINAPI*)(int,int);using Clip=BOOL(WINAPI*)(const RECT*);
inline Function original=nullptr;inline Clip original_clip=nullptr;
struct Slot {void** address;void* original;void* replacement;};
inline std::vector<Slot> slots;
inline std::atomic<HWND> editor_window{nullptr};
inline std::atomic<uint64_t> warps{0},clips{0},passed{0};
inline bool owns_focus(HWND target,HWND foreground) {
    if(!target || !IsWindowVisible(target))return false;
    const auto owner=GetWindow(target,GW_OWNER);
    if(!owner || (foreground!=target && foreground!=owner))return false;
    DWORD pid=0,owner_pid=0;GetWindowThreadProcessId(target,&pid);GetWindowThreadProcessId(owner,&owner_pid);
    wchar_t name[64]{};GetClassNameW(target,name,64);
    return pid==GetCurrentProcessId() && owner_pid==pid && wcscmp(name,L"DS2FX.Forge.Menu.1")==0;
}
inline BOOL warp(HWND target,HWND foreground,int x,int y){if(owns_focus(target,foreground)){++warps;return TRUE;}++passed;return original?original(x,y):FALSE;}
inline BOOL clip(HWND target,HWND foreground,const RECT* bounds){if(owns_focus(target,foreground)){++clips;return TRUE;}return original_clip?original_clip(bounds):FALSE;}
inline BOOL WINAPI guarded(int x,int y){return warp(editor_window.load(),GetForegroundWindow(),x,y);}
inline BOOL WINAPI guarded_clip(const RECT* bounds){return clip(editor_window.load(),GetForegroundWindow(),bounds);}
inline void patch(HMODULE module){
    auto base=reinterpret_cast<BYTE*>(module);const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return;
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);if(nt->Signature!=IMAGE_NT_SIGNATURE)return;
    const auto dir=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];if(!dir.VirtualAddress)return;
    for(auto* d=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+dir.VirtualAddress);d->Name;++d){
        if(_stricmp(reinterpret_cast<const char*>(base+d->Name),"USER32.dll") || !d->OriginalFirstThunk)continue;
        auto* names=reinterpret_cast<IMAGE_THUNK_DATA64*>(base+d->OriginalFirstThunk);auto* targets=reinterpret_cast<IMAGE_THUNK_DATA64*>(base+d->FirstThunk);
        for(int i=0;names[i].u1.AddressOfData;++i){
            if(IMAGE_SNAP_BY_ORDINAL64(names[i].u1.Ordinal))continue;
            auto* entry=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+names[i].u1.AddressOfData);
            const auto name=reinterpret_cast<const char*>(entry->Name);
            void* expected=nullptr;void* replacement=nullptr;
            if(!strcmp(name,"SetCursorPos")){expected=reinterpret_cast<void*>(original);replacement=reinterpret_cast<void*>(guarded);}
            if(!strcmp(name,"ClipCursor")){expected=reinterpret_cast<void*>(original_clip);replacement=reinterpret_cast<void*>(guarded_clip);}
            if(!expected)continue;auto candidate=reinterpret_cast<void**>(&targets[i].u1.Function);
            if(*candidate!=expected)continue;DWORD protection=0;if(!VirtualProtect(candidate,sizeof(void*),PAGE_READWRITE,&protection))continue;
            const auto before=InterlockedCompareExchangePointer(candidate,replacement,expected);DWORD ignored=0;VirtualProtect(candidate,sizeof(void*),protection,&ignored);
            if(before==expected)slots.push_back({candidate,expected,replacement});
        }
    }
}
inline bool install(){
    if(!slots.empty())return true;
    const auto exe=GetModuleHandleW(nullptr);if(hash_file(module_path(exe))!=game_hash)return false;
    original=reinterpret_cast<Function>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetCursorPos"));
    original_clip=reinterpret_cast<Clip>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"ClipCursor"));patch(exe);
    // Reviewed local graphics binaries also import cursor warp/clipping APIs.
    // Authentication is exact and runtime-only. Seamless is never patched.
    const std::pair<const wchar_t*,const char*> graphics[]{
        {L"winmm.dll","856f14f111118090ae3eb2a9d11b6f5693e5df8ee99c75404f73d21e5a917a16"},
        {L"dxgi.dll","c3a88fd51368dbe17b4b71311bd47486f0a72d048a8fda64c7f3e567b84181b8"},
        {L"ReShade68.dll","680b304aed496ca0c7987ea207922a0fac3846742da19e1ed357d7f409beeda7"}};
    for(const auto& item:graphics){const auto module=GetModuleHandleW(item.first);if(!module)continue;const auto path=module_path(module);
        if(path.parent_path()==module_path(exe).parent_path() && hash_file(path)==item.second)patch(module);}
    return !slots.empty();
}
inline void uninstall(){editor_window=nullptr;for(const auto& slot:slots){DWORD protection=0;
    if(VirtualProtect(slot.address,sizeof(void*),PAGE_READWRITE,&protection)){InterlockedCompareExchangePointer(slot.address,slot.original,slot.replacement);DWORD ignored=0;VirtualProtect(slot.address,sizeof(void*),protection,&ignored);}}slots.clear();}
}

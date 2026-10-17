#include "lantern_le_brightness.h"
#include "common.h"
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace fx::lantern_le_brightness {
namespace {
constexpr char supported_hash[]="c3a88fd51368dbe17b4b71311bd47486f0a72d048a8fda64c7f3e567b84181b8";
constexpr size_t image_size=0xc65000, value_rva=0xbde4c0;
constexpr DWORD timestamp=0x6a014f46;
bool copy_read(uintptr_t address,void* destination,size_t bytes) noexcept {
    if(!address || bytes>std::numeric_limits<uintptr_t>::max()-address) return false;
    SIZE_T copied=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),destination,bytes,&copied)!=FALSE && copied==bytes;
}
struct ImageReader {
    uintptr_t base; size_t bytes;
    bool read(size_t rva,void* destination,size_t count) const noexcept {
        return rva<=bytes && count<=bytes-rva && base<=std::numeric_limits<uintptr_t>::max()-rva && copy_read(base+rva,destination,count);
    }
    template<class T> bool read(size_t rva,T& destination) const noexcept {return read(rva,&destination,sizeof(destination));}
};
template<size_t N> bool matches(const ImageReader& image,size_t rva,const unsigned char (&expected)[N]) noexcept {
    std::array<unsigned char,N> actual{};
    return image.read(rva,actual.data(),N) && std::memcmp(actual.data(),expected,N)==0;
}
bool validate_image(const ImageReader& image) noexcept {
    IMAGE_DOS_HEADER dos{}; IMAGE_NT_HEADERS64 nt{};
    if(image.bytes!=image_size || !image.read(0,dos) || dos.e_magic!=IMAGE_DOS_SIGNATURE || dos.e_lfanew!=0x148 ||
        !image.read(static_cast<size_t>(dos.e_lfanew),nt) || nt.Signature!=IMAGE_NT_SIGNATURE ||
        nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64 || nt.FileHeader.NumberOfSections!=7 ||
        nt.FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER64) || nt.FileHeader.TimeDateStamp!=timestamp ||
        nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt.OptionalHeader.SizeOfImage!=image_size ||
        nt.OptionalHeader.SizeOfHeaders!=0x400) return false;
    // Require the expected readable/executable code and writable data sections;
    // the float must belong to .data, never an executable or foreign allocation.
    std::array<IMAGE_SECTION_HEADER,7> sections{};
    if(!image.read(0x148+sizeof(IMAGE_NT_HEADERS64),sections.data(),sizeof(sections))) return false;
    const auto& code=sections[0]; const auto& data=sections[2];
    if(std::memcmp(code.Name,".text\0\0\0",8)!=0 || code.VirtualAddress!=0x1000 || code.Misc.VirtualSize!=0x894f8e || code.Characteristics!=0x60000020 ||
        std::memcmp(data.Name,".data\0\0\0",8)!=0 || data.VirtualAddress!=0xbba000 || data.Misc.VirtualSize!=0x48750 || data.Characteristics!=0xc0000040 ||
        value_rva<data.VirtualAddress || value_rva+sizeof(float)>static_cast<size_t>(data.VirtualAddress)+data.Misc.VirtualSize) return false;
    // These independent xrefs all resolve to the same float. Both consuming
    // instructions are guarded, so a modified renderer is unsupported as well.
    constexpr unsigned char gui[]={0x48,0x8d,0x15,0xf6,0x3f,0xa0,0x00};
    constexpr unsigned char render_read[]={0xf3,0x0f,0x10,0x15,0x4b,0x0e,0xa9,0x00};
    constexpr unsigned char render_multiply[]={0xf3,0x44,0x0f,0x59,0x0d,0x06,0x27,0xa5,0x00};
    constexpr unsigned char config_save[]={0xf3,0x0f,0x10,0x35,0x8a,0x9e,0x96,0x00};
    constexpr unsigned char config_load[]={0xf3,0x0f,0x11,0x05,0x91,0xcd,0x96,0x00};
    constexpr char label[]="Character lantern intensity";
    constexpr char key[]="character_lantern_intensity";
    std::array<char,sizeof(label)> actual_label{}; std::array<char,sizeof(key)> actual_key{};
    return matches(image,0x1da4c3,gui) && matches(image,0x14d66d,render_read) && matches(image,0x18bdb1,render_multiply) &&
        matches(image,0x27462e,config_save) && matches(image,0x271727,config_load) &&
        image.read(0xacaeb8,actual_label.data(),actual_label.size()) && std::memcmp(actual_label.data(),label,sizeof(label))==0 &&
        image.read(0xadfa70,actual_key.data(),actual_key.size()) && std::memcmp(actual_key.data(),key,sizeof(key))==0;
}
bool readable_image_region(uintptr_t base,size_t rva,size_t count,bool executable) noexcept {
    MEMORY_BASIC_INFORMATION memory{};
    if(base>std::numeric_limits<uintptr_t>::max()-rva) return false;
    const auto address=base+rva;
    if(!VirtualQuery(reinterpret_cast<const void*>(address),&memory,sizeof(memory)) || memory.State!=MEM_COMMIT || memory.Type!=MEM_IMAGE ||
        reinterpret_cast<uintptr_t>(memory.AllocationBase)!=base || (memory.Protect&(PAGE_NOACCESS|PAGE_GUARD))) return false;
    const auto begin=reinterpret_cast<uintptr_t>(memory.BaseAddress);
    if(address<begin || address-begin>memory.RegionSize || count>memory.RegionSize-(address-begin)) return false;
    const auto protection=memory.Protect&0xff;
    return executable ? protection==PAGE_EXECUTE_READ || protection==PAGE_EXECUTE_READWRITE || protection==PAGE_EXECUTE_WRITECOPY
        : protection==PAGE_READONLY || protection==PAGE_READWRITE || protection==PAGE_WRITECOPY;
}
}
const char* status_name(Status value) noexcept {
    switch(value) {
    case Status::not_initialized:return "not_initialized";
    case Status::absent:return "absent";
    case Status::ready:return "ready";
    case Status::unsupported_version:return "unsupported_version";
    case Status::invalid_image:return "invalid_image";
    case Status::unreadable:return "unreadable";
    case Status::invalid_value:return "invalid_value";
    }
    return "unknown";
}
Adapter::~Adapter(){if(module_)FreeLibrary(module_);}
Status Adapter::initialize() noexcept {
    const auto detected=detection_status();
    if(detected!=Status::not_initialized && detected!=Status::absent) return detected;
    try {
        // Full path is essential: the LE proxy and Windows' dxgi may both be
        // loaded, and basename-only GetModuleHandleEx is ambiguous.
        const auto expected=fx::module_path().parent_path()/L"dxgi.dll";
        return initialize_loaded_path(expected.c_str());
    } catch(...) {return status_=Status::unreadable;}
}
Status Adapter::initialize_loaded_path(const wchar_t* absolute_path) noexcept {
    const auto detected=detection_status();
    if(detected!=Status::not_initialized && detected!=Status::absent) return detected;
    // GetModuleHandleEx increments the reference count but never loads a DLL.
    if(!absolute_path || !GetModuleHandleExW(0,absolute_path,&module_)) return status_=Status::absent;
    try {
        if(fx::hash_file(fx::module_path(module_))!=supported_hash) return status_=Status::unsupported_version;
        const auto base=reinterpret_cast<uintptr_t>(module_);
        if(!readable_image_region(base,0,0x400,false) || !readable_image_region(base,value_rva,sizeof(float),false) ||
            !readable_image_region(base,0x14d66d,8,true) || !readable_image_region(base,0x18bdb1,9,true) ||
            !readable_image_region(base,0x1da4c3,7,true) || !validate_image({base,image_size})) return status_=Status::invalid_image;
        value_=base+value_rva;
        return status_=Status::ready;
    } catch(...) {return status_=Status::unreadable;}
}
Sample Adapter::sample() const noexcept {
    const auto detected=detection_status();
    if(detected!=Status::ready) return {1.0f,detected};
    float value=0;
    if(!copy_read(value_,&value,sizeof(value))) return {1.0f,Status::unreadable};
    if(!std::isfinite(value)) return {1.0f,Status::invalid_value};
    return {std::clamp(value,0.0f,10.0f),Status::ready};
}
#ifdef DS2FX_FIXTURE
Status Adapter::initialize_loaded_path_for_fixture(const wchar_t* absolute_path) noexcept {return initialize_loaded_path(absolute_path);}
Status Adapter::initialize_owned_fixture(void* image,size_t bytes,const char* disk_hash) noexcept {
    const auto detected=detection_status();
    if(detected!=Status::not_initialized && detected!=Status::absent) return detected;
    if(!disk_hash || std::strcmp(disk_hash,supported_hash)!=0) return status_=Status::unsupported_version;
    const auto base=reinterpret_cast<uintptr_t>(image);
    if(!validate_image({base,bytes})) return status_=Status::invalid_image;
    value_=base+value_rva;
    return status_=Status::ready;
}
#endif
}

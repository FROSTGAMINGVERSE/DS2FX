#include "../src/lantern_le_brightness.h"
#include "../src/common.h"
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>

namespace {
constexpr char hash[]="c3a88fd51368dbe17b4b71311bd47486f0a72d048a8fda64c7f3e567b84181b8";
using namespace fx::lantern_le_brightness;
template<class T> void put(std::vector<unsigned char>& image,size_t rva,const T& value){std::memcpy(image.data()+rva,&value,sizeof(value));}
template<size_t N> void bytes(std::vector<unsigned char>& image,size_t rva,const unsigned char (&value)[N]){std::memcpy(image.data()+rva,value,N);}
std::vector<unsigned char> fixture(){
    std::vector<unsigned char> image(0xc65000);
    IMAGE_DOS_HEADER dos{};dos.e_magic=IMAGE_DOS_SIGNATURE;dos.e_lfanew=0x148;put(image,0,dos);
    IMAGE_NT_HEADERS64 nt{};nt.Signature=IMAGE_NT_SIGNATURE;nt.FileHeader.Machine=IMAGE_FILE_MACHINE_AMD64;nt.FileHeader.NumberOfSections=7;
    nt.FileHeader.SizeOfOptionalHeader=sizeof(IMAGE_OPTIONAL_HEADER64);nt.FileHeader.TimeDateStamp=0x6a014f46;
    nt.OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR64_MAGIC;nt.OptionalHeader.SizeOfImage=0xc65000;nt.OptionalHeader.SizeOfHeaders=0x400;put(image,0x148,nt);
    std::array<IMAGE_SECTION_HEADER,7> sections{};
    std::memcpy(sections[0].Name,".text",5);sections[0].VirtualAddress=0x1000;sections[0].Misc.VirtualSize=0x894f8e;sections[0].Characteristics=0x60000020;
    std::memcpy(sections[2].Name,".data",5);sections[2].VirtualAddress=0xbba000;sections[2].Misc.VirtualSize=0x48750;sections[2].Characteristics=0xc0000040;put(image,0x148+sizeof(nt),sections);
    bytes(image,0x1da4c3,{0x48,0x8d,0x15,0xf6,0x3f,0xa0,0x00});bytes(image,0x14d66d,{0xf3,0x0f,0x10,0x15,0x4b,0x0e,0xa9,0x00});
    bytes(image,0x18bdb1,{0xf3,0x44,0x0f,0x59,0x0d,0x06,0x27,0xa5,0x00});bytes(image,0x27462e,{0xf3,0x0f,0x10,0x35,0x8a,0x9e,0x96,0x00});
    bytes(image,0x271727,{0xf3,0x0f,0x11,0x05,0x91,0xcd,0x96,0x00});
    const char label[]="Character lantern intensity",key[]="character_lantern_intensity";put(image,0xacaeb8,label);put(image,0xadfa70,key);put(image,0xbde4c0,0.1f);
    return image;
}
}
int main(){try{
    using namespace fx::lantern_le_brightness;
    Adapter untouched;fx::require(untouched.sample().multiplier==1.0f && untouched.sample().status==Status::not_initialized,"Uninitialized defaults safely");
    Adapter absent;const auto absent_status=absent.initialize();
    fx::require(absent_status==Status::absent && absent.sample().multiplier==1.0f,"Standalone host has no dxgi; no DLL gets loaded");
    fx::require(GetModuleHandleW(L"dxgi.dll")==nullptr,"Detection must not load dxgi");
    fx::require(absent.initialize()==absent_status,"Absent retry remains read-only");
    auto image=fixture();const auto original=image;
    Adapter valid;fx::require(valid.initialize_owned_fixture(image.data(),image.size(),hash)==Status::ready,"Owned exact PE fixture accepted");
    for(const auto expected : {0.0f,0.1f,1.0f,4.98f,10.0f}) {put(image,0xbde4c0,expected);auto s=valid.sample();fx::require(s.status==Status::ready && s.multiplier==expected,"Live value, including zero and greater than one");}
    put(image,0xbde4c0,-5.0f);fx::require(valid.sample().multiplier==0,"Finite lower clamp");
    put(image,0xbde4c0,25.0f);fx::require(valid.sample().multiplier==10,"Finite upper clamp");
    for(const auto bad : {std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}) {
        put(image,0xbde4c0,bad);auto s=valid.sample();fx::require(s.multiplier==1 && s.status==Status::invalid_value,"Nonfinite default/status");
    }
    put(image,0xbde4c0,0.1f);fx::require(valid.sample().status==Status::ready && image==original,"Samples do not modify image and recover after invalid float");
    for(const auto rva : {size_t{0},size_t{0x148},size_t{0x148+8},size_t{0x160},size_t{0x198},size_t{0x274},size_t{0x2c4},size_t{0x1da4c3},size_t{0x14d66d},size_t{0x18bdb1},size_t{0x27462e},size_t{0x271727},size_t{0xacaeb8},size_t{0xadfa70}}){
        image[rva]^=1;Adapter corrupt;fx::require(corrupt.initialize_owned_fixture(image.data(),image.size(),hash)==Status::invalid_image && corrupt.sample().multiplier==1,"Corrupt metadata/instruction/identity rejected");image[rva]^=1;
    }
    Adapter wrong_hash;fx::require(wrong_hash.initialize_owned_fixture(image.data(),image.size(),"wrong")==Status::unsupported_version,"Exact disk hash required");
    Adapter truncated;fx::require(truncated.initialize_owned_fixture(image.data(),image.size()-1,hash)==Status::invalid_image,"Truncated image rejected");
    Adapter inaccessible;fx::require(inaccessible.initialize_owned_fixture(reinterpret_cast<void*>(1),image.size(),hash)==Status::invalid_image,"Bad address safely rejected");
    Adapter published;std::atomic<bool> finished=false,publication_ok=true;
    std::thread reader([&]{while(!finished.load()){const auto s=published.sample();if((s.status==Status::ready && s.multiplier!=0.1f) || (s.status!=Status::ready && s.multiplier!=1))publication_ok=false;}});
    published.initialize_owned_fixture(image.data(),image.size(),hash);finished=true;reader.join();
    fx::require(publication_ok && published.sample().status==Status::ready,"Concurrent initialization publishes value before ready");
    auto protected_image=VirtualAlloc(nullptr,image.size(),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);fx::require(protected_image!=nullptr,"Allocate unreadable-value fixture");
    std::memcpy(protected_image,image.data(),image.size());
    {Adapter protected_value;fx::require(protected_value.initialize_owned_fixture(protected_image,image.size(),hash)==Status::ready,"Protected-memory fixture initially valid");
        DWORD previous=0;auto value=static_cast<unsigned char*>(protected_image)+0xbde4c0;
        fx::require(VirtualProtect(value,4,PAGE_NOACCESS,&previous)!=FALSE,"Protect fixture value page");
        const auto missing=protected_value.sample();fx::require(missing.status==Status::unreadable && missing.multiplier==1,"Unreadable runtime float falls back safely");
        DWORD discarded=0;VirtualProtect(value,4,previous,&discarded);}
    VirtualFree(protected_image,0,MEM_RELEASE);
    // An ordinary Windows dxgi cannot be mistaken for the game-adjacent proxy.
    auto system_dxgi=LoadLibraryExW(L"dxgi.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);fx::require(system_dxgi!=nullptr,"Load OS dxgi fixture");
    fx::require(absent.initialize()==Status::absent,"System dxgi does not satisfy exact game-adjacent module path");
    const auto system_path=fx::module_path(system_dxgi);
    {Adapter unknown;fx::require(unknown.initialize_loaded_path_for_fixture(system_path.c_str())==Status::unsupported_version && unknown.sample().multiplier==1,"Unknown real exact-path module defaults without addressing private RVA");FreeLibrary(system_dxgi);HMODULE retained=nullptr;fx::require(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,system_path.c_str(),&retained)!=FALSE,"Adapter owns module reference during reads");}
    auto late_dxgi=LoadLibraryExW(L"dxgi.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);fx::require(late_dxgi!=nullptr,"Load late OS dxgi fixture");
    fx::require(absent.initialize_loaded_path_for_fixture(system_path.c_str())==Status::unsupported_version && absent.sample().multiplier==1,"Absent can detect a later exact-path module on worker retry");FreeLibrary(late_dxgi);
    std::cout<<"PASS LE brightness: absent/no-load/late retry, exact hash and PE/section/instruction guards, live0..10, clamps/nonfinite, no image writes, concurrent publication, inaccessible float, unknown module and lifetime\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}

#include "../src/common.h"
int main() {
    const auto host=fx::module_path().parent_path()/"ds2fx_bridge_fixture_host.exe";
    auto command=L"\""+host.wstring()+L"\"";
    STARTUPINFOW si{}; si.cb=sizeof(si); PROCESS_INFORMATION pi{};
    if(!CreateProcessW(host.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,host.parent_path().c_str(),&si,&pi)) return 1;
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    // Mimic a launcher that exits while its game continues.
    return 0;
}

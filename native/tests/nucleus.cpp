#include "../src/nucleus_context.h"
#include <iostream>
int main() {
    try {
        using namespace fx;
        auto root=fs::temp_directory_path()/"ds2fx-nucleus-contract";
        require(nucleus::instance_directory(root/"Instance0",root),"instance0");
        require(nucleus::instance_directory(root/"Instance3",root),"instance3");
        for(auto s:{"Instance4","Instance-1","Instance00","other","Instance0/child"})
            require(!nucleus::instance_directory(root/s,root),"invalid instance accepted");
        auto game=root/"Instance0"/"DarkSoulsII.exe";
        nucleus::verify_other(root/"Instance1"/"DarkSoulsII.exe",game,root);
        for(auto p:{game,root.parent_path()/"other"/"DarkSoulsII.exe"}) {
            bool refused=false; try { nucleus::verify_other(p,game,root); } catch(...) { refused=true; }
            require(refused,"duplicate or foreign process accepted");
        }
        bool refused=false; try {nucleus::validate(game,root);} catch(...) {refused=true;}
        require(refused,"launch without session accepted");
        std::cout<<"PASS Nucleus instance bounds, duplicate and foreign process rejection, missing session rejection\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what(); return 1; }
}

#include <winsock2.h>
#include "../src/offline_network.h"
#include <iostream>
int main() {
    try {
        fx::offline_network::install();
        WSADATA data{};
        const int result=WSAStartup(MAKEWORD(2,2),&data);
        if(result!=WSASYSNOTREADY||fx::offline_network::blocked_calls()!=1||data.wVersion!=0) return 1;
        std::cout<<"PASS process-local offline guard: actual WSAStartup returns WSASYSNOTREADY\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what(); return 1; }
}

#include "InetAddress.h"
#include <strings.h>
#include <string.h>
#include <iostream>
InetAddress::InetAddress(uint16_t port, std::string ip){
    bzero(&addr_, sizeof addr_); //清零 addr_结构体
    addr_.sin_family = AF_INET;  // 设置地址族为IPv4
    addr_.sin_port = htons(port); // 设置端口号，并将主机字节序转换为网络字节序。 htons：将16位（2字节）整数从主机字节序转换为网络字节序（big-endian）。
    addr_.sin_addr.s_addr = inet_addr(ip.c_str()); //将字符串形式的IP地址转换为二进制网络字节序，并赋值给 addr_.sin_addr.s_addr
}

std::string InetAddress::toIp() const{

    // addr_
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf); //将二进制形式的IP地址转换为点分十进制形式，并存储在 buf 中。
    return buf;
}

std::string InetAddress::toIpPort() const{
    // ip:port
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf); //同样是将二进制形式的IP地址转换为点分十进制形式，并存储在 buf 中。
    size_t end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port); //将网络字节序的端口号转换为主机字节序。保证大小端统一
    sprintf(buf + end, ":%u", port);
    return buf;
}

uint16_t InetAddress::toPort() const{
    return ntohs(addr_.sin_port); //将网络字节序的端口号转换为主机字节序。保证大小端统一
}

// int main(){
//     InetAddress addr(8080);
//     std::cout << addr.toIpPort() << std::endl;
// }
#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
                        // “最多 32 位”IPv4 地址前缀，用于匹配数据报的目标地址
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
                        // IP地址的前缀长度
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
                    // 下一跃点的 IP 地址。如果网络直接连接到路由器，则为空（在这种情况下，下一跳地址应为数据报的最终目的地）。
//! \param[in] interface_num The index of the interface to send the datagram out on.
                        // 要向其发送数据报的接口的索引。
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    // DUMMY_CODE(route_prefix, prefix_length, next_hop, interface_num);
    // Your code here.
    // 将路由添加到路由表中
    router_table.push_back({route_prefix, prefix_length, next_hop, interface_num});
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    DUMMY_CODE(dgram);
    // Your code here.

    // 工作流程：
    auto match_one=router_table.end();  
    // 在路由表查询，是否有匹配的网络接口
    for(auto i=router_table.begin();i!=router_table.end();i++){
        // 最长前缀匹配 
        // 目的地址的最长prefix_length与route_prefix的最长prefix_length相同
        // IP地址的前缀为0，或前缀匹配成功
        if(i->prefix_length==0||(i->route_prefix^dgram.header().dst)>>(32-i->prefix_length)==0)
        {
            // 在匹配的路由中，路由器选择prefix_length最长的路由
            if(match_one==router_table.end()||match_one->prefix_length<i->prefix_length){
                match_one=i; // 更新最匹配的
            }
        }
    }
    // 有匹配上的
    if(match_one!=router_table.end()){
        // 路由器减少数据报的TTL（存活时间），ttl是否为0
        if(dgram.header().ttl==0);//已经为0，丢弃
        else{
            dgram.header().ttl--;
            if(dgram.header().ttl>0){
                // 将修改后的数据报从接口发送到适当的下一跳
                AsyncNetworkInterface &interface=_interfaces[match_one->interface_idx];
                if (match_one->next_hop.has_value())
                    interface.send_datagram(dgram, match_one->next_hop.value());
                else
                    interface.send_datagram(dgram, Address::from_ipv4_numeric(dgram.header().dst));
            }//减少到0的情况将数据报丢弃
        }
    }//没有匹配的情况将数据报丢弃
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}

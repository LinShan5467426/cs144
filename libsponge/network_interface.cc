#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

// template <typename... Targs>
// void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    // 工作流程：
    // 先查找ARP表，看有没有对应的mac地址
    const auto &result=arp_table.find(next_hop_ip); 
    if(result!=arp_table.end()){// 如果有 
        // 直接组装以太网帧并发送 // 创建一个类型为EthernetHeader::TYPE_IPv4的以太网帧
        EthernetFrame eth_frame;
        eth_frame.header()={result->second.first,_ethernet_address,EthernetHeader::TYPE_IPv4};
        eth_frame.payload()=dgram.serialize();
        _frames_out.push(eth_frame);
    }
    else{// 若之前未发送过ARP请求
        if(arp_request_ip.find(next_hop_ip)==arp_request_ip.end()){
            // 组装ARP请求包发送
            ARPMessage arp_request;
            arp_request.opcode = ARPMessage::OPCODE_REQUEST;
            arp_request.sender_ethernet_address = _ethernet_address;
            arp_request.sender_ip_address = _ip_address.ipv4_numeric();
            arp_request.target_ethernet_address = {};
            arp_request.target_ip_address = next_hop_ip;
            
            EthernetFrame eth_frame; 
            eth_frame.header()={ETHERNET_BROADCAST,_ethernet_address,EthernetHeader::TYPE_ARP};
            eth_frame.payload()=arp_request.serialize();
            _frames_out.push(eth_frame);
            
            arp_request_ip[next_hop_ip]=arp_request_ttl; // 加入发送请求的列表并标记ttl
            // 将数据报文放进等待队列，等待响应
            waiting_ip_datagrams.push_back(make_pair(next_hop,dgram));
        }else // 如果之前发送过了ARP请求 将数据报文放进等待队列，等待响应
            waiting_ip_datagrams.push_back(make_pair(next_hop,dgram)); 
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    // 工作流程：
    // 不是发送到该网络接口的？// 地址不匹配且不为广播
    if(frame.header().dst!=_ethernet_address && frame.header().dst!=ETHERNET_BROADCAST)
        return {}; // 直接过滤掉不响应
	// 看是不是IPv4或者ARP包 
    if(frame.header().type!=EthernetHeader::TYPE_IPv4 && frame.header().type!=EthernetHeader::TYPE_ARP)
        return {}; // 都不是，不响应
	// 是IPv4
    if(frame.header().type==EthernetHeader::TYPE_IPv4){
        // 看能否正常解析，能则将报文返回给调用者
        InternetDatagram datagram;
        if(datagram.parse(frame.payload())==ParseResult::NoError){
            return datagram;
        }else return {};
    }
	// 是ARP包
    else if(frame.header().type==EthernetHeader::TYPE_ARP){
        // 看能否正常解析
        ARPMessage message;
        if(message.parse(frame.payload()) == ParseResult::NoError){
            // 更新ARP表
            arp_table[message.sender_ip_address]=make_pair(message.sender_ethernet_address, arp_entry_ttl);
            // 删除原先在等待队列中的数据
            for(auto i=waiting_ip_datagrams.begin();i!=waiting_ip_datagrams.end(); ){
                if(i->first.ipv4_numeric()==message.sender_ip_address){
                    send_datagram(i->second, i->first);
                    i=waiting_ip_datagrams.erase(i);
                }else ++i;
            } 
            // 删除发送对应的报文
            arp_request_ip.erase(message.sender_ip_address);
            //若是请求包，还需发送ARP应答包
            if(message.opcode == ARPMessage::OPCODE_REQUEST && _ip_address.ipv4_numeric()==message.target_ip_address){
                // 组装ARPMessage和EthernetFrame，发送
                ARPMessage arp_reply;
                arp_reply.opcode = ARPMessage::OPCODE_REPLY;
                arp_reply.sender_ethernet_address = _ethernet_address;
                arp_reply.sender_ip_address = _ip_address.ipv4_numeric();
                arp_reply.target_ethernet_address = message.sender_ethernet_address;
                arp_reply.target_ip_address = message.sender_ip_address;

                EthernetFrame eth_frame;
                eth_frame.header()={message.sender_ethernet_address,_ethernet_address,EthernetHeader::TYPE_ARP};
                eth_frame.payload()=arp_reply.serialize();
                _frames_out.push(eth_frame);
            }
        }else return {};
    }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { 
    // 工作流程：
	// 将ARP表中过期的条目删除，检查每一条
    for(auto i=arp_table.begin();i!=arp_table.end(); ){
        // 如果过期了，直接删除
        if(i->second.second<=ms_since_last_tick)
            i=arp_table.erase(i);
        else{ // 如果没过期，更新时间
            i->second.second-=ms_since_last_tick;
            ++i;
        }
    }
    // 如果 ARP 等待队列中的 ARP 请求过期，则重新发送 ARP 请求；检查每一条
    for(auto i=arp_request_ip.begin();i!=arp_request_ip.end(); ){
        // 如果过期了
        if(i->second<=ms_since_last_tick){
            // 重新组装ARP请求包和对应以太网帧并发送
            ARPMessage arp_request;
            arp_request.opcode = ARPMessage::OPCODE_REQUEST;
            arp_request.sender_ethernet_address = _ethernet_address;
            arp_request.sender_ip_address = _ip_address.ipv4_numeric();
            arp_request.target_ethernet_address = {};
            arp_request.target_ip_address = i->first;
            EthernetFrame eth_frame;
            eth_frame.header()={ETHERNET_BROADCAST,_ethernet_address,EthernetHeader::TYPE_ARP};
            eth_frame.payload()=arp_request.serialize();
            _frames_out.push(eth_frame);
            // 更新时间
            i->second=arp_request_ttl;
        }
        else{
            // 如果没过期，更新时间
            i->second-=ms_since_last_tick;
            ++i;
        }
    }
}


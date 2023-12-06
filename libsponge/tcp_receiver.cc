#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

// template <typename... Targs>
// void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//当前 TCPReceiver 大体上有三种状态， 分别是
// 1. LISTEN，此时 SYN 包尚未抵达。可以通过 _set_syn_flag 标志位来判断是否在当前状态
// 2. SYN_RECV, 此时 SYN 抵达。只能判断当前不在 1、3状态时才能确定在当前状态
// 3. FIN_RECV, 此时 FIN 抵达。可以通过 ByteStream end_input 来判断是否在当前状态

//接收TCPsegment，并重新组装字节流
void TCPReceiver::segment_received(const TCPSegment &seg) {
    // DUMMY_CODE(seg);
    uint64_t abs_seqno=0; //绝对序列号
    //Listen状态
    if(!_set_syn_flag&&!seg.header().syn) return;//没有找到目标起始
    else if(!_set_syn_flag&&seg.header().syn){
        _set_syn_flag=true; //标记以监听到，进入接收状态
        _isn=WrappingInt32(seg.header().seqno.raw_value());//syn的序列号标记为isn
        abs_seqno=1;//syn携带用户数据，则此处为payload的第一个部分
        uint64_t index=abs_seqno-1;
        _reassembler.push_substring(seg.payload().copy(),index,seg.header().fin);
    }
    //不是的话需要组装字节流进行处理
    else{
        //放入流重组器
        abs_seqno=unwrap(seg.header().seqno,_isn,_last_read_abs_idx);
        uint64_t index=abs_seqno-1;
        _reassembler.push_substring(seg.payload().copy(),index,seg.header().fin);
    }
    //更新标志位
    //写入字节流的总个数，即处理完成的总字节数
    _last_read_abs_idx=stream_out().bytes_written(); //-1

}

//确定应该发回发送者的信号，以进行数据确认和流量控制
//即需要知道ack确认号和接收窗口

//提供要返回的ack确认号（序列号）
optional<WrappingInt32> TCPReceiver::ackno() const { 
    //判断当前是否为Listen状态
    //是，返回空
    if(!_set_syn_flag) return nullopt;
    //否，查询尚未获取到的第一个字节的流索引，将流索引转换为序列号（32位的）返回
    // 注意：如果当前处于 FIN_RECV 状态
    else if(stream_out().input_ended()) 
        return wrap(_last_read_abs_idx,_isn)+2; //需要加上 FIN 标志长度
    else return wrap(_last_read_abs_idx,_isn)+1;
}

//窗口大小为总容量减去已置入output的大小（可发送-已发送）
size_t TCPReceiver::window_size() const { 
    //计算总容量_capacity与流重组器_reassembler中已存数据量( _out_put )的差值
    return _capacity-stream_out().buffer_size(); 
}

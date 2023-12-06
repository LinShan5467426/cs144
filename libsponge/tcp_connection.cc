#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

// 处理_sender中待发送的包
void TCPConnection::_send_segment(){
    //  依次处理_sender中segments_out（push进的待发送的包）
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        // 如果是要发回给发送者的信号
        if (_receiver.ackno().has_value()) {
            // 设置 ACK、确认应答号和接收窗口大小
            seg.header().ack=true;
            seg.header().ackno=_receiver.ackno().value();
            seg.header().win=_receiver.window_size();
        }
        //从_sender中移到connection进行处理
        _segments_out.push(seg);
    }
}

void TCPConnection::segment_received(const TCPSegment &seg) { 
    
    // 有没有内容或者是不是有syn or fin标识
    // 表示是否需要发送确认收到的回复ack
    // 实际上表示是否需要发送空包回复，过程中对内容不为0的进行重新标记
    bool need_send_ack = seg.length_in_sequence_space(); 
    // you code here.
    //如果已标记终止连接，则不继续接收
    if(!active()) return; 
    //你需要考虑到ACK包、RST包等多种情况
    // 如果收到RST包 不限时间节点 设置标记暴力退出
    if(seg.header().rst){
        // 将入站流和出站流都设置为错误状态
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        // 并永久终止连接
        _is_active=false;
    }
    // 否则进行进一步识别
    // 把这个段交给TCPReceiver
    _receiver.segment_received(seg);
    // 重置计时器
    _time_since_last_segment_received=0;
    // 如果设置了ACK标志 
    // 接收到回复ACK包，则更新传送信息
    if(seg.header().ack){
        // 告诉TCPSender它关心的传入段的字段 ackno和window_size
        _sender.ack_received(seg.header().ackno,seg.header().win);
        // 上述函数同时实现了fill_window继续发送后续包的操作,反应ackno和window_size的更新
        
        // 有待发送的数据报文，不需要单独使用空包回复ack
        if(!_sender.segments_out().empty()&&need_send_ack)
            need_send_ack=false;
    }

    //状态变化(按照个人的情况可进行修改)
    // 如果是 LISEN 到了 SYN
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED) {
        // 此时肯定是第一次调用 fill_window，因此会发送 SYN + ACK
        connect();
        return;
    }

    // 判断 TCP 断开连接时是否时需要等待
    // CLOSE_WAIT
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED)
        _linger_after_streams_finish = false;

    // 如果到了准备断开连接的时候。服务器端先断
    // CLOSED
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && !_linger_after_streams_finish) {
        _is_active = false;
        return;
    }

    // 如果收到的数据包里没有任何数据，则这个数据包可能只是为了 keep-alive
    if (need_send_ack)
        _sender.send_empty_segment();

    // 接收到了一般数据包
    // 处理发送端，发送其待发送数据包（含ack）
    _send_segment();

}

bool TCPConnection::active() const { return _is_active; }

// 将数据写入输入端的字节流中
size_t TCPConnection::write(const string &data) {
    // DUMMY_CODE(data);
    size_t write_size=_sender.stream_in().write(data); // 写入并标记数量
    _sender.fill_window(); // 更新窗口
    _send_segment(); // 处理_sender中的发送数据包
    return write_size; 
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    // DUMMY_CODE(ms_since_last_tick);
    // 告诉TCPSender时间的流逝
    _sender.tick(ms_since_last_tick);
    // 如果连续重传的次数超过上限TCPConfig::MAX_RETX_ATTEMPTS
    if(_sender.consecutive_retransmissions()>_cfg.MAX_RETX_ATTEMPTS){
        // 停止将要进行的重传
        _sender.segments_out().pop();
        // 发送一个重置段给对端（设置了RST标志的空段）
        // 设置并发送数据包
        TCPSegment seg;
        seg.header().rst=true;
        _segments_out.push(seg);
        // 将入站流和出站流都设置为错误状态
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        // 并永久终止连接
        _is_active=false;
    }
    // 处理重传内容 发送
    _send_segment();
    // 更新计时器时间
    _time_since_last_segment_received+=ms_since_last_tick;
    // 如有必要，结束连接
    // 满足条件1-3 && _linger_after_streams_finish为true
    // 需要等待足够的时间（10 * _cfg.rt_timeout）后结束连接
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && 
        _linger_after_streams_finish && _time_since_last_segment_received>=10*_cfg.rt_timeout){
        _is_active = false;
        _linger_after_streams_finish=false;
        return;
    }
}

// 结束发送端的输入字节流
void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input(); //标记结束_sender数据流的写入状态
    // 在输入流结束后，必须立即发送 FIN
    _sender.fill_window(); //更新窗口
    _send_segment(); //处理_sender中的数据包
}

// 通过发送 SYN 段来启动连接
void TCPConnection::connect() {
    //第一次调用 fill_window
    _sender.fill_window();//在判断接收包为ack并告诉_sender时已实现？
    _is_active=true;//标记状态为active正常活动
    _send_segment();//发送 SYN + ACK
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            // 设置并发送数据包
            TCPSegment seg;
            seg.header().rst=true;
            _segments_out.push(seg);
            // 将入站流和出站流都设置为错误状态
            _receiver.stream_out().set_error();
            _sender.stream_in().set_error();
            // 并永久终止连接
            _is_active=false;
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <algorithm>
// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _retransmisson_timeout(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    uint64_t Size=_ack_no_received+_window_size_received-next_seqno_absolute();//最多仍可发送的大小;
    if (bytes_in_flight()>_window_size_received) Size=0;//停止发送，先等待确认
    else if(_window_size_received==0) Size=1;//窗口为0时，按1对待（注意1指的是可发送大小）
    if (Size==0||_fin_sent) return;
    TCPSegment seg;
    if(!_syn_sent){
        seg.header().syn=true;
        //设置seqno和payload （syn没有payload则不进行存储）
        seg.header().seqno=next_seqno();
        _next_seqno+=seg.length_in_sequence_space();//更新_next_seqno下一个待传送的绝对序列号,+一个段所占用的序列号的总数
        _bytes_in_flight+=seg.length_in_sequence_space();//更新未确认字节数
        _syn_sent=true;//更新标记
        segments_out().push(seg);//发送
        segments_target().push(seg);//追踪
        //开启计时器
        if (!_timer_running) {
            _timer_running = true;
            _total_time = 0;
        }
    }else if(!stream_in().eof()){
        //循环向前更新窗口
        // uint64_t Size=_ack_no_received+_window_size_received-next_seqno_absolute();
        while(Size>0&&!stream_in().buffer_empty()){
            //窗口有空余并且待发送字节流非空
            //设置seqno和payload （syn没有payload则不进行存储）
            seg.header().seqno=next_seqno();
            uint64_t max_len=min(Size,min(TCPConfig::MAX_PAYLOAD_SIZE, stream_in().buffer_size()));
            seg.payload() = stream_in().read(max_len);
            //是否结束,若满足条件则增加FIN
            if(seg.length_in_sequence_space()<Size&&stream_in().eof()){
                _fin_sent=true;
                seg.header().fin=true;
            }
            //更新_next_seqno下一个待传送的绝对序列号,+一个段所占用的序列号的总
            _next_seqno+=seg.length_in_sequence_space();
            _bytes_in_flight+=seg.length_in_sequence_space();//更新未确认字节数
            // Size=_ack_no_received+_window_size_received-_next_seqno;//更新窗口大小
            Size-=seg.length_in_sequence_space();//更新
            segments_out().push(seg);//发送
            segments_target().push(seg);//追踪
            // 开启计时器
            if (!_timer_running) {
                _timer_running = true;
                _total_time = 0;
            }
        }
    } 
    //若满足条件则增加FIN
    if(!_fin_sent&&stream_in().eof()){
        // uint64_t Size=_ack_no_received+_window_size_received-next_seqno_absolute();//最多仍可发送的大小
        if(Size>0){
            //发送fin
            seg.header().fin=true;
            //设置seqno和payload （syn没有payload则不进行存储）
            seg.header().seqno=next_seqno();
            _next_seqno+=seg.length_in_sequence_space();//更新_next_seqno下一个待传送的绝对序列号,+一个段所占用的序列号的总数
            _bytes_in_flight+=seg.length_in_sequence_space();//更新未确认字节数
            // Size=_ack_no_received+_window_size_received-next_seqno_absolute();//最多仍可发送的大小
            Size-=seg.length_in_sequence_space();
            _fin_sent=true;//更新标记
            segments_out().push(seg);//发送
            segments_target().push(seg);//追踪
            //开启计时器
            if (!_timer_running) {
                _timer_running = true;
                _total_time = 0;
            }
        }
        else return;
    }else if(_fin_sent) return;//全部发送完成
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//处理TCPReceiver传入的ackno和window size
//以追踪接收者当前的接收状态，并检测丢包的情况
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    // DUMMY_CODE(ackno, window_size); 
    uint64_t abs_ackno=unwrap(ackno, _isn, _next_seqno);//ackno的绝对序列号
// 如果传入的 ack 是不可靠的（ack_seqno大于next_seqno），则直接丢弃
    if(abs_ackno>next_seqno_absolute()) return;
    if(abs_ackno>=_ack_no_received) _ack_no_received=abs_ackno;//保存ack
// 遍历数据结构（用来存储发送的segment），如果一个发送的segment已经被成功接收
    TCPSegment seg;
    while(!segments_target().empty()){
        seg=segments_target().front();//从队列头开始遍历
        uint64_t x=unwrap(seg.header().seqno,_isn,_next_seqno)+seg.length_in_sequence_space();
        if(x<=abs_ackno){
            // ==》从数据结果中将该segment丢弃
            _bytes_in_flight-=seg.length_in_sequence_space();//更新未确定的字节数!!
            segments_target().pop();
            // ==》重置重传计时器
            _total_time=0;_timer_running=true;
            // ==》将RTO重置为初始值
            _retransmisson_timeout=_initial_retransmission_timeout;
        }else break;//后面的未被确认，结束遍历
    }
    // 重置连续重传计数器
    _consecutive_retransmissions=0;
    // 更新窗口大小 (位置)
    _window_size_received=static_cast<uint64_t>(window_size);
    //如果没有发送但未确认的报文，关闭计时器
    if (bytes_in_flight() == 0) {
        _timer_running = false;
    }
// 调用fill_window继续发送数据（更新窗口大小后，接收方可能有了新的接收空间，所以应该再次发送数据）
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _total_time += ms_since_last_tick;//更新计时
    // 遍历追踪列表，如果存在发送中的数据包，并且重传计时器超时
    if(_total_time>=_retransmisson_timeout&&!segments_target().empty()){
        // ==》重置重传计时器
        _total_time=0;
        // ==》重传尚未被 TCP 接收方完全确认的序列号最小的段
        segments_out().push(segments_target().front());
        // ==》如果对方接收窗口大小不为0（说明网络拥堵）
        if(_window_size_received || _segments_target.front().header().syn){
            //==》超时重传时间 RTO 的值加倍，即*2
            _retransmisson_timeout*=2;
            //==》连续重传计时器增加
            _consecutive_retransmissions++;
        }
    }
    if (segments_target().empty()) {
        _timer_running = false;
    }//全部收到确认信息后停止计时

}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

//发送一个空段（将其push进入待发送队列中）
void TCPSender::send_empty_segment() {
    //生成并发送一个payload长度为零的TCPSegment
    //即设置完序列号直接发送即可，无需其他操作。
    TCPSegment segment;
    //并且序列号设置正确
    segment.header().seqno=next_seqno();
    _segments_out.push(segment);
}

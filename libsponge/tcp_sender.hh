#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};//TCPSender要发送的段的出站队列

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;//RTO超时重传时间

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;//尚未发送的传出字节流

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};//要发送的下一个字节的绝对序列号

    //********************************************
    uint64_t _bytes_in_flight=0;//表示未被确认的字节数（已发送和已确认中间部分）
    uint64_t _ack_no_received=0;//读取到的ack确认号，第一个未确认的绝对序列号//流索引
    size_t _window_size_received=1;//接收方的窗口大小
    unsigned int _consecutive_retransmissions=0;//连续重传计时器，连续发生的连续重传次数
    uint64_t _syn_sent=false;//表示是否发送syn
    uint64_t _fin_sent=false;//表示是否发送fin
    bool _timer_running=false;//表示是否开始计时
    unsigned int _retransmisson_timeout=0;//超时重传时间
    unsigned int _total_time=0;//总时长
    std::queue<TCPSegment> _segments_target{};//存储已发数据包的数据结构
    //**********************************************

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    std::queue<TCPSegment> &segments_target() { return _segments_target; }
    // std::queue<TCPSegment> &segments_waiting() { return _segments_waiting; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
// 给定一个绝对序列号（n）和初始序列号（isn），生成序列号
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    // DUMMY_CODE(n, isn);
    // return WrappingInt32{0};

    uint32_t result;
    result=isn.raw_value()+static_cast<uint32_t>(n);
    return WrappingInt32(result);
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
// 给定序列号（n）、初始序列号（isn）和checkpoint的绝对序列号，计算n对应的绝对序列号
uint64_t abs(uint64_t d1, uint64_t d2) {return (d1>d2)?(d1-d2):d2-d1;}//求距离
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    // DUMMY_CODE(n, isn, checkpoint);
    // return {};

    WrappingInt32 check_n=wrap(checkpoint,isn);//check的（相对）序列号
    int32_t delta=n-check_n; //发现重载运算符
    //共存在三种情况，二者在同一范围内，分散在两范围内（一大一小）
    //分别讨论
    uint64_t res1=checkpoint+delta;//在同一范围内
    uint64_t res2=checkpoint+(0x100000000+delta); //跨范围偏大
    uint64_t res3=checkpoint-(0x100000000+delta); //跨范围偏小
    uint64_t d1 = abs(res1, checkpoint);
    uint64_t d2 = abs(res2, checkpoint);
    uint64_t d3 = abs(res3, checkpoint);
    uint64_t result;
    if(min(min(d1,d2),d3)==d1) result=res1;
    else if(min(min(d1,d2),d3)==d2) result=res2;
    else result=res3;
    return static_cast<uint64_t>(result);
}


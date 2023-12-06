#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

// template <typename... Targs>
// void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

// *********************以下为lab2进行的修改*************************

ByteStream::ByteStream(const size_t capacity) 
    : Byte_Stream(), Capacity(capacity), wsize(0), rsize(0), input_end(false), _error(false) {}

// 写操作
size_t ByteStream::write(const string &data) {
    size_t i = 0;
    size_t wsize_new = 0;
    // 当空间仍有容量剩余且输入操作未结束时可进行写入操作
    while(this->remaining_capacity()&&(!this->input_ended())){
        if(i<data.length()){
            Byte_Stream.push_back(data[i]); //按字节写入字节流中
            wsize_new++; //更新写入字节数
            i++;
        }else break;
    }
    wsize += wsize_new; //更新已写入数据大小

    return wsize_new; //返回新写入的字节数
}

//! \param[in] len bytes will be copied from the output side of the buffer
// peek为从头部开始读取指定数量的字节
string ByteStream::peek_output(const size_t len) const {
    string peek_data;
    //可读取的字节数无法超过buffer存储的大小
    size_t l = min(this->buffer_size(),len); 
    // for(size_t i = 0; i < l; i++){
    //     peek_data += *(Byte_Stream.begin()+i); //按顺序读出字节 
    // }
    //按顺序读出字节内容
    peek_data = std::string(Byte_Stream.begin(),Byte_Stream.begin()+l);
    return peek_data; //返回读出的数据
}

//! \param[in] len bytes will be removed from the output side of the buffer
// pop为弹出指定数量的字节
void ByteStream::pop_output(const size_t len) { 
    size_t i = 0;
    // 当buffer内仍有字节可弹出（读取）
    while(!this->buffer_empty()){
        //按顺序pop掉头部对应数量的字节
        if(i<len){
            Byte_Stream.pop_front(); 
            rsize++; //更新已读出的数据大小
            i++;
        }else break;
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string read_data; 
    read_data = this->peek_output(len); //从头部开始读取指定数量的字节
    this->pop_output(len); //弹出指定数量的字节
    return read_data; //返回读出的数据
}

void ByteStream::end_input() {input_end = true; } //修改输入结束状态为是

bool ByteStream::input_ended() const { return input_end; } //输出输入结束状态

size_t ByteStream::buffer_size() const { return Byte_Stream.size(); } //数据流大小

bool ByteStream::buffer_empty() const { return this->buffer_size()==0; } //数据流大小是否为零

bool ByteStream::eof() const { return this->input_ended() && this->buffer_empty(); }

size_t ByteStream::bytes_written() const { return wsize; } //write的字节数

size_t ByteStream::bytes_read() const { return rsize; } //read的字节数

size_t ByteStream::remaining_capacity() const { return Capacity-this->buffer_size(); } //剩余容量

// *****************************************************************
// #include "socket.hh"
#include "tcp_sponge_socket.hh"
#include "util.hh"

#include <cstdlib>
#include <iostream>

using namespace std;

// // 向指定IP地址发送HTTP GET请求，然后输出所有响应。
// void get_URL(const string &host, const string &path) {
//     // Your code here.

//     // 建立连接
//     // You will need to connect to the "http" service on
//     // the computer whose name is in the "host" string,
//     const Address webserver(host,"http");
//     // then request the URL path given in the "path" string.
//     TCPSocket sock;
//     sock.connect(webserver);
//     sock.write("GET " + path + " HTTP/1.1");
//     sock.write("\r\n");
//     sock.write("Host: " + host);
//     sock.write("\r\n");
//     // Connection:close这句代码必须包含在客户端的请求中。
//     sock.write("Connection: close");
//     sock.write("\r\n");
//     sock.write("\r\n");

//     // 输出
//     // Then you'll need to print out everything the server sends back,
//     // (not just one call to read() -- everything) until you reach
//     // the "eof" (end of file).
//     // 确保从服务器读取和打印所有的输出，即直到套接字到达“EOF”（文件的末尾），才停止打印输出。
//     while(!sock.eof()){
//         cout<<sock.read();
//     }
    
//     //关闭连接
//     sock.close();


// 	// // 输出错误信息
//     // cerr << "Function called: get_URL(" << host << ", " << path << ").\n";
//     // cerr << "Warning: get_URL() has not been implemented yet.\n";
// }

//********************以下为lab5修改********************
void get_URL(const string &host, const string &path){
    const Address webserver(host,"http");
    FullStackSocket sock;
    sock.connect(webserver);
    sock.write("GET " + path + " HTTP/1.1");
    sock.write("\r\n");
    sock.write("Host: " + host);
    sock.write("\r\n");
    // Connection:close这句代码必须包含在客户端的请求中。
    sock.write("Connection: close");
    sock.write("\r\n");
    sock.write("\r\n");
    while(!sock.eof()){
        cout<<sock.read();
    }
    // sock.close();
    sock.wait_until_closed();
}
//*****************************************************

int main(int argc, char *argv[]) {
    try {
        if (argc <= 0) {
            abort();  // For sticklers: don't try to access argv[0] if argc <= 0.
        }

        // The program takes two command-line arguments: the hostname and "path" part of the URL.
        // Print the usage message unless there are these two arguments (plus the program name
        // itself, so arg count = 3 in total).
        if (argc != 3) {
            cerr << "Usage: " << argv[0] << " HOST PATH\n";
            cerr << "\tExample: " << argv[0] << " stanford.edu /class/cs144\n";
            return EXIT_FAILURE;
        }

        // Get the command-line arguments.
        const string host = argv[1];
        const string path = argv[2];

        // Call the student-written function.
        get_URL(host, path);
    } catch (const exception &e) {
        cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

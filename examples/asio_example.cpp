
#include "asio.hpp"
#include "dbase/log/log.h"
#include <iostream>

using asio::ip::tcp;

/*
int main()
{
    asio::io_context io;

    tcp::acceptor acceptor(
            io,
            tcp::endpoint(tcp::v4(), 8888));

    DBASE_LOG_INFO("listen 8888");

    while (true)
    {
        tcp::socket socket(io);
        acceptor.accept(socket);

        DBASE_LOG_INFO("client connected: {}", socket.remote_endpoint().address().to_string());
    }
}
*/

int main()
{
    asio::io_context ioc;

    tcp::resolver resolver(ioc);
    auto endpoints = resolver.resolve("www.bilibili.com", "http");

    tcp::socket socket(ioc);
    asio::connect(socket, endpoints);

    {
        std::string path = "/";
        std::string request = "GET " + path + " HTTP/1.1\r\n";
        request += std::string("") + "Host: " + "www.bilibili.com" + "\r\n";
        request += "Accept: */*\r\n";
        request += "Connection: close\r\n\r\n";

        asio::write(socket, asio::buffer(request));

        char buffer[1024] = {0};
        asio::error_code ec;
        while (size_t len = socket.read_some(asio::buffer(buffer), ec))
        {
            if (ec == asio::error::eof)
            {
                break;  // Connection closed cleanly by peer.
            }
            else if (ec)
            {
                throw asio::system_error(ec);  // Some other error.
            }

            std::cout.write(buffer, len);
        }
    }

    return 0;
}
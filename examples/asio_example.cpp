
#include "ext/asio/asio.hpp"
#include "dbase/log/log.h"
#include <iostream>

using asio::ip::tcp;

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
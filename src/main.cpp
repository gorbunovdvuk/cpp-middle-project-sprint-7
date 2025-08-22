#include "headers.h"

#include <iostream>
#include <string_view>
#include <sstream>
#include <ranges>
#include <print>
#include <charconv>


#include <asio.hpp>
#include <asio/co_spawn.hpp>
#include <asio/io_context.hpp>

#include <boost/program_options.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace {

void init_logging(std::string app, std::string level = "debug") {
    auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console->set_pattern("[%Y-%m-%d %H:%M:%S.%e %z] [%^%l%$] [tid %t] [%s:%# %!] [%n] %v");
    auto logger = std::make_shared<spdlog::logger>(
        app, spdlog::sinks_init_list{console}
    );
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::from_str(level));
    spdlog::flush_on(spdlog::level::level_enum::debug);
}

asio::awaitable<void> session(asio::ip::tcp::socket client_socket) {
    std::exception_ptr eptr;
    try {
        SPDLOG_INFO("session started");
        std::string request_buffer;
        co_await asio::async_read_until(client_socket, asio::dynamic_buffer(request_buffer), delimiter, asio::use_awaitable);
        SPDLOG_INFO("received request, size: {}", request_buffer.size());

        auto host_port = findHostPort(request_buffer);
        if (!host_port.has_value()) {
            throw std::runtime_error("Host line not found");
        }
        auto [host_str, port_str] = host_port.value();

        std::error_code host_ec;
        auto host = asio::ip::make_address_v4(host_str, host_ec);
        if (host_ec) {
            SPDLOG_ERROR("Host address host_str: {} message: {}", host_str, host_ec.message());
            throw std::runtime_error("Invalid host address host_str");
        }

        uint16_t port = 0;
        if (auto [port_end, port_ec] = std::from_chars(port_str.begin(), port_str.end(), port); port_ec != std::errc{}) {
            SPDLOG_ERROR("Port error: {}", port);
            throw std::runtime_error("Invalid port number");
        }

        SPDLOG_INFO("host: {} port: {}", host.to_string(), port);

        asio::ip::tcp::socket origin_server_socket{client_socket.get_executor()};
        co_await origin_server_socket.async_connect(asio::ip::tcp::endpoint{host, port}, asio::use_awaitable);
        SPDLOG_INFO("connected to origin_server host: {} port: {}", host.to_string(), port);

        co_await asio::async_write(origin_server_socket, asio::buffer(request_buffer.data(), request_buffer.size()), asio::use_awaitable);
        SPDLOG_INFO("sent {} bytes to origin_server host: {} port: {}", request_buffer.size(), host.to_string(), port);

        std::string response_buffer;
        co_await asio::async_read_until(origin_server_socket, asio::dynamic_buffer(response_buffer), delimiter, asio::use_awaitable);
        SPDLOG_INFO("received response from origin_server, size: {} host: {} port: {}", response_buffer.size(), host.to_string(), port);

        size_t response_eor = response_buffer.find(delimiter);

        size_t content_length = findContentLength(response_buffer).value();
        SPDLOG_INFO("content-length: {}", content_length);

        co_await asio::async_write(client_socket, asio::buffer(response_buffer.data(), response_buffer.size()), asio::use_awaitable);
        SPDLOG_INFO("sent bytes to client, size: {} host: {} port: {}", response_buffer.size(), client_socket.local_endpoint().address().to_string(), client_socket.local_endpoint().port());

        content_length -= response_buffer.size() - (response_eor + 4);
        SPDLOG_INFO("left to send, size: {}", content_length);
        while (content_length > 0) {
            std::vector<char> buffer(content_length);
            size_t read = co_await origin_server_socket.async_read_some(asio::buffer(buffer.data(), content_length), asio::use_awaitable);
            SPDLOG_INFO("read bytes, size: {}", read);
            if (read == 0) {
                break;
            }
            co_await asio::async_write(client_socket, asio::buffer(buffer.data(), read), asio::use_awaitable);
            SPDLOG_INFO("write bytes, size: {}", read);
            content_length -= read;
            SPDLOG_INFO("left to send, size: {}", content_length);
        }
    } catch (const std::exception& e) {
        SPDLOG_ERROR("exception {}", e.what());
        eptr = std::current_exception();
    }
    asio::error_code ec;
    client_socket.close(ec);
    SPDLOG_INFO("socket close");
    if (eptr) {
        std::rethrow_exception(eptr);
    }
    co_return;
}

class Server {
public:
    Server(asio::any_io_executor io_executor, uint16_t port):
        endpoint_{asio::ip::tcp::v4(), port},
        acceptor_(io_executor, endpoint_)
    {
        SPDLOG_INFO("starting server at host: {} port: {}", endpoint_.address().to_string(), endpoint_.port());
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(acceptor_.get_executor(), [this](asio::error_code ec, asio::ip::tcp::socket client_socket) {
            if (ec) {
                if (ec != asio::error::operation_aborted) {
                    SPDLOG_ERROR("accept error: {}", ec.message());
                }
                return;
            }
            const auto remote_endpoint = client_socket.remote_endpoint();
            SPDLOG_INFO("accepted connection from {} {}", remote_endpoint.address().to_string(),
                         remote_endpoint.port());
            asio::co_spawn(acceptor_.get_executor(), session(std::move(client_socket)), [](std::exception_ptr eptr) {
                try {
                    if (eptr) {
                        std::rethrow_exception(eptr);
                    }
                } catch (const std::exception &e) {
                    SPDLOG_ERROR("session error: {}", e.what());
                }
            });
            do_accept();
        });
    }

    asio::ip::tcp::endpoint endpoint_;
    asio::ip::tcp::acceptor acceptor_;
};

}  // namespace

int main(int argc, char *argv[]) {
    try {
        init_logging("AsyncHttpProxy");

        boost::program_options::options_description desc("Options");
        desc.add_options()
            ("help,h", "help message")
            ("port,p", boost::program_options::value<uint16_t>()->required());

        boost::program_options::positional_options_description positional;
        positional.add("port", 1);

        auto parsed_options = boost::program_options::command_line_parser(argc, argv)
            .options(desc)
            .positional(positional)
            .run();

        boost::program_options::variables_map vm;
        boost::program_options::store(parsed_options, vm);

        if (vm.contains("help")) {
            std::println("Usage: {} [options] <port>", argv[0]);
            std::println("\n{}", (std::stringstream() << desc).str());
            return 0;
        }

        boost::program_options::notify(vm);

        asio::io_context io_context;
        Server server(io_context.get_executor(), vm["port"].as<uint16_t>());
        io_context.run();
    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

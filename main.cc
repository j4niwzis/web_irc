#include <cstdio>
import web_irc.core;
import web_irc.config;
import web_irc.http_handlers;
import std;
import boost;
namespace asio = boost::asio;
using asio::awaitable;
int main() {
  try {
    asio::io_context io(1);
    std::unordered_map<std::string, web_irc::connection_entry> connections;
    int next_nicid = 1;
    auto handler = [&](boost::beast::tcp_stream& stream, web_irc::request_t req) -> awaitable<void> {
      co_await web_irc::handle_request(io, connections, next_nicid, stream, std::move(req));
    };
    asio::co_spawn(io, web_irc::listener(handler), [](std::exception_ptr ep) {
      if(ep) {
        try {
          std::rethrow_exception(ep);
        } catch(std::exception const& e) {
          std::println(stderr, "[listener] fatal: {}", e.what());
        }
      }
    });
    io.run();
  } catch(std::exception const& e) {
    std::println(stderr, "[fatal] {}", e.what());
    return 1;
  }
}

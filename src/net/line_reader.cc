export module web_irc.net.line_reader;
import boost;
import std;

namespace web_irc::net {

namespace asio = boost::asio;

using asio::awaitable;

export class line_reader {
 public:
  explicit line_reader(asio::ip::tcp::socket socket) : socket_(std::move(socket)) {
  }

  std::optional<std::string_view> try_read_line() {
    auto pos = buffer_.find('\n');
    if(pos == std::string::npos) {
      return std::nullopt;
    }

    std::string_view line(&buffer_[0], pos);
    if(!line.empty() && line.back() == '\r') {
      line = line.substr(0, line.size() - 1);
    }

    line_ = std::string(line);
    buffer_.erase(0, pos + 1);

    return std::string_view(line_);
  }

  awaitable<void> read() {
    char buf[4096];
    std::size_t bytes = co_await socket_.async_read_some(asio::buffer(buf), asio::use_awaitable);

    if(bytes == 0) {
      throw std::runtime_error("Connection closed");
    }

    buffer_.append(buf, bytes);
  }

  awaitable<std::string_view> read_line() {
    while(true) {
      auto line = try_read_line();
      if(line) {
        co_return line.value();
      }
      co_await read();
      {
        auto line = try_read_line();
        if(line) {
          co_return line.value();
        }
      }
      co_await socket_.async_wait(asio::ip::tcp::socket::wait_read, asio::use_awaitable);
    }
  }

  asio::ip::tcp::socket& socket() {
    return socket_;
  }
  void close() {
    return socket_.close();
  }

 private:
  asio::ip::tcp::socket socket_;
  std::string buffer_;
  std::string line_;
};

}  // namespace web_irc::net

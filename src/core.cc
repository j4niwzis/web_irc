module;
#include <cstdio>
export module web_irc.core;
import std;
export import boost;
import web_irc.config;

inline void log_exception(std::exception_ptr ep, std::FILE* out, std::string_view tag) {
  if(!ep) {
    return;
  }
  try {
    std::rethrow_exception(ep);
  } catch(std::exception const& e) {
    std::println(out, "[{}] {}", tag, e.what());
  }
}

namespace web_irc {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using asio::awaitable;

export using request_t = http::request<http::string_body>;

export std::string utc_timestamp() {
  auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
  auto hms = std::chrono::hh_mm_ss(now.time_since_epoch() % std::chrono::days(1));
  return std::format("[{:02d}:{:02d}:{:02d}]", hms.hours().count(), hms.minutes().count(), hms.seconds().count());
}

export template <class Body, class Fields>
awaitable<void> send_message(beast::tcp_stream& stream, http::response<Body, Fields>&& res) {
  co_await http::async_write(stream, res, asio::use_awaitable);
}

export awaitable<void> send_text_response(
    beast::tcp_stream& stream, unsigned version, http::status status, std::string body, bool keep_alive = false) {
  http::response<http::string_body> res{status, version};
  res.set(http::field::server, config.short_name);
  res.set(http::field::content_type, "text/plain; charset=utf-8");
  res.keep_alive(keep_alive);
  res.body() = std::move(body);
  res.prepare_payload();

  co_await send_message(stream, std::move(res));
}

export awaitable<void> send_html_response(
    beast::tcp_stream& stream, unsigned version, http::status status, std::string body, bool keep_alive = false) {
  http::response<http::string_body> res{status, version};
  res.set(http::field::server, config.short_name);
  res.set(http::field::content_type, "text/html; charset=utf-8");
  res.keep_alive(keep_alive);
  res.body() = std::move(body);
  res.prepare_payload();

  co_await send_message(stream, std::move(res));
}

export awaitable<void> send_binary_response(beast::tcp_stream& stream,
                                            unsigned version,
                                            http::status status,
                                            std::string_view content_type,
                                            std::vector<std::uint8_t> data,
                                            bool keep_alive = false) {
  using body_t = http::vector_body<std::uint8_t>;
  http::response<body_t> res{status, version};
  res.set(http::field::server, config.short_name);
  res.set(http::field::content_type, content_type);
  res.keep_alive(keep_alive);
  res.body() = std::move(data);
  res.prepare_payload();

  co_await send_message(stream, std::move(res));
}

export awaitable<void> send_not_found(beast::tcp_stream& stream, unsigned version) {
  co_await send_text_response(stream, version, http::status::not_found, "not found\n", false);
}

export awaitable<void> send_redirect(beast::tcp_stream& stream, unsigned version, std::string_view location) {
  http::response<http::string_body> res{http::status::see_other, version};
  res.set(http::field::server, config.short_name);
  res.set(http::field::location, location);
  res.keep_alive(false);
  res.body() = "";
  res.prepare_payload();

  co_await send_message(stream, std::move(res));
}

export awaitable<void> send_stream_chunk(beast::tcp_stream& stream, std::string_view chunk) {
  // http chunked body piece
  co_await asio::async_write(stream.socket(), http::make_chunk(asio::buffer(chunk)), asio::use_awaitable);
}

export awaitable<void> send_stream_chunk(beast::tcp_stream& stream, std::string chunk) {
  co_await asio::async_write(stream.socket(), http::make_chunk(asio::buffer(chunk)), asio::use_awaitable);
}

export awaitable<void> send_stream_last_chunk(beast::tcp_stream& stream) {
  co_await asio::async_write(stream.socket(), http::make_chunk_last(), asio::use_awaitable);
}

export awaitable<request_t> read_request(beast::tcp_stream& stream, beast::flat_buffer& buffer) {
  http::request_parser<http::string_body> parser;
  parser.body_limit(1024 * 1024);

  co_await boost::beast::http::async_read(stream, buffer, parser, asio::use_awaitable);
  co_return parser.release();
}

export awaitable<void> serve_connection(auto&& handle_request, boost::asio::ip::tcp::socket socket) {
  boost::beast::tcp_stream stream(std::move(socket));
  {
    boost::beast::error_code ec;
    stream.socket().set_option(boost::asio::socket_base::keep_alive(true), ec);
  }
  boost::beast::flat_buffer buffer;

  try {
    auto req = co_await read_request(stream, buffer);
    co_await handle_request(stream, std::move(req));
  } catch(std::exception const& e) {
    std::println(stderr, "[session error] {}", e.what());
  }

  boost::beast::error_code ec;
  stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
}

export boost::asio::awaitable<void> listener(auto&& handle_request) {
  auto ex = co_await boost::asio::this_coro::executor;
  auto address = static_cast<std::string>(config.connection.address);
  boost::asio::ip::tcp::acceptor acceptor(ex, {boost::asio::ip::make_address(address), config.connection.port});

  std::println("[listen] http://{}:{}/", address, config.connection.port);

  for(;;) {
    boost::asio::ip::tcp::socket socket = co_await acceptor.async_accept(boost::asio::use_awaitable);

    auto remote = socket.remote_endpoint();
    std::println("[accept] {}:{}", remote.address().to_string(), remote.port());

    boost::asio::co_spawn(ex, serve_connection(handle_request, std::move(socket)), [](std::exception_ptr ep) {
      log_exception(ep, stderr, "spawn error");
    });
  }
}
}  // namespace web_irc

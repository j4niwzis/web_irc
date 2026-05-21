export module web_irc.irc.client;
import std;
import boost;
export import mirc;
import web_irc.net.line_reader;
import web_irc.config;
import web_irc.utils.range_string_formatter;
import web_irc.utils.lazy_format;

namespace web_irc::irc {

namespace asio = boost::asio;

using asio::awaitable;

export class client {
 public:
  struct config {
    std::string nickname;
    std::string username;
    std::string realname;
    std::string real_ip;
  };
  explicit client(asio::ip::tcp::socket socket, config cfg) : line_reader_(std::move(socket)), cfg_(std::move(cfg)) {
  }
  template <auto = 0>
  awaitable<void> connect() {
    auto [... events] =
        std::tuple{mirc::client::nick{.nickname = cfg_.nickname}, mirc::client::user{.username = cfg_.username, .realname = cfg_.realname}};
    if constexpr(constexpr auto ptr = std::get_if<web_irc::webirc::use>(&web_irc::config.webirc)) {
      const auto& w = *ptr;
      co_await send(mirc::client::webirc{.password = w.password, .login = cfg_.username, .hostname = w.gateway, .ip_address = cfg_.real_ip},
                    events...);
      co_return;
    }

    co_await send(events...);
  }
  std::optional<mirc::event_t> try_read_event() {
    auto line = line_reader_.try_read_line();
    if(!line) {
      return std::nullopt;
    }
    auto ev = mirc::parse_event(*line);
    if(!ev) {
      throw std::runtime_error("[irc] parse_event failed for: " + std::string{*line});
    }
    return *std::move(ev);
  }
  awaitable<mirc::event_t> read_event() {
    for(;;) {
      auto event = try_read_event();
      if(!event) {
        co_await read();
        continue;
      }
      co_return *std::move(event);
    }
  }
  awaitable<void> read() {
    co_await line_reader_.read();
  }

  template <typename... Ts>
  awaitable<void> send(Ts... events) {
    std::string chunk;
    (event_work_to(std::back_inserter(chunk), std::move(events)), ...);
    std::println("[irc] Sending {}", chunk.subview(0, chunk.size() - 2));
    co_await send_chunk(std::move(chunk));
  }
  asio::ip::tcp::socket& socket() {
    return line_reader_.socket();
  }
  config& config() {
    return cfg_;
  }
  void close() {
    line_reader_.close();
  }

 private:
  awaitable<void> send_chunk(std::string_view chunk) {
    co_await asio::async_write(line_reader_.socket(), asio::buffer(chunk), asio::use_awaitable);
  }

  template <typename T>
  auto event_work_to(auto it, T event) {
    return std::format_to(it, "{}\r\n", std::move(event));
  }
  auto event_work_to(auto it, mirc::client::nick event) {
    cfg_.nickname = event.nickname;
    return std::format_to(it, "{}\r\n", std::move(event));
  }
  template <std::ranges::range T>
  auto event_work_to(auto it, T range)
    requires std::same_as<std::ranges::range_value_t<T>, char>
  {
    return std::ranges::copy(std::string_view{"\r\n"}, std::ranges::copy(range, it).out);
  }
  template <std::ranges::range T>
  auto event_work_to(auto it, T range) {
    return std::format_to(it, "{}", utils::range_string_formatter{std::move(range) | std::views::transform([&](auto event) {
                                                                    return utils::lazy_format(utils::string_tag<"{}\r\n">{},
                                                                                              std::move(event));
                                                                  })});
  }
  net::line_reader line_reader_;
  struct config cfg_;
};

}  // namespace web_irc::irc

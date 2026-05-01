#include <cstdio>
import web_irc.core;
import web_irc.gen;
import web_irc.irc;
import web_irc.range_string_formatter;
import std;
import boost;
import ctre;

namespace http = boost::beast::http;
namespace asio = boost::asio;
using asio::awaitable;

struct connection_entry {
  std::shared_ptr<web_irc::irc_client> irc;
};

static std::string generate_uuid() {
  static std::mutex mtx;
  static std::mt19937_64 gen([] {
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    if(!urandom) {
      throw std::runtime_error("Cannot open /dev/urandom");
    }

    alignas(std::uint64_t) char arr[sizeof(std::uint64_t)];
    urandom.read(arr, sizeof(std::uint64_t));
    if(!urandom) {
      throw std::runtime_error("Failed to read from /dev/urandom");
    }

    return std::bit_cast<std::uint64_t>(arr);
  }());

  std::lock_guard<std::mutex> lock(mtx);

  constexpr std::array<char, 16> hex_chars = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

  auto random_hex = [&]() {
    return hex_chars[std::uniform_int_distribution<>(0, 15)(gen)];
  };

  auto random_variant = [&]() {
    return hex_chars[std::uniform_int_distribution<>(8, 11)(gen)];
  };

  auto generate_segment = [&](int count) {
    return std::views::iota(0, count) | std::views::transform([&](int) {
             return random_hex();
           });
  };

  return std::format("{}-{}-4{}-{}{}-{}",
                     web_irc::range_string_formatter{generate_segment(8)},
                     web_irc::range_string_formatter{generate_segment(4)},
                     web_irc::range_string_formatter{generate_segment(3)},
                     random_variant(),
                     web_irc::range_string_formatter{generate_segment(3)},
                     web_irc::range_string_formatter{generate_segment(12)});
}

static std::string url_decode(std::string_view sv) {
  std::string r;
  r.reserve(sv.size());
  for(std::size_t i = 0; i < sv.size(); ++i) {
    if(sv[i] == '+') {
      r += ' ';
      continue;
    } else if(sv[i] == '%' && i + 2 < sv.size()) {
      auto hex_val = [](char c) -> int {
        if(c >= '0' && c <= '9')
          return c - '0';
        if(c >= 'a' && c <= 'f')
          return c - 'a' + 10;
        if(c >= 'A' && c <= 'F')
          return c - 'A' + 10;
        return -1;
      };
      int high = hex_val(sv[i + 1]);
      int low = hex_val(sv[i + 2]);
      if(high >= 0 && low >= 0) {
        r += static_cast<char>((high << 4) | low);
        i += 2;
        continue;
      }
    }
    r += sv[i];
  }
  return r;
}

static std::string extract_value(std::string_view data, std::string_view key) {
  for(auto part : data | std::views::split('&')) {
    auto eq = std::ranges::find(part, '=');
    if(eq == part.end())
      continue;
    if(std::string_view(part.begin(), eq) == key) {
      return url_decode(std::string_view(std::next(eq), part.end()));
    }
  }
  return {};
}

namespace {

constexpr auto client_css = [] {
  static constexpr char data[] = {
#embed "static/client.css"
  };
  return std::string_view(data, sizeof(data));
}();

constexpr auto shadow_css = [] {
  static constexpr char data[] = {
#embed "static/shadow.css"
  };
  return std::string_view(data, sizeof(data));
}();

constexpr auto page_css = [] {
  static constexpr char data[] = {
#embed "static/page.css"
  };
  return std::string_view(data, sizeof(data));
}();

}  // namespace

static std::string_view trim_left(std::string_view sv) {
  auto pos = sv.find_first_not_of(' ');
  return pos == std::string_view::npos ? "" : sv.substr(pos);
}

static std::string ensure_hash_prefix(std::string_view sv) {
  return sv.empty() || sv[0] == '#' ? std::string(sv) : std::format("#{}", sv);
}

static std::string form_placeholder(std::string_view channel) {
  if(channel.empty() || channel == "status")
    return "Message";
  return std::format("Message for #{}", channel);
}

struct parsed_cmd {
  std::string_view name;
  std::string_view args;
};

static std::optional<parsed_cmd> parse_slash_cmd(std::string_view text) {
  if(text.empty() || text[0] != '/')
    return std::nullopt;
  constexpr auto pattern = ctll::fixed_string{R"(/(\w+)\s*(.*))"};
  if(auto m = ctre::match<pattern>(text)) {
    return parsed_cmd{m.template get<1>().to_view(), m.template get<2>().to_view()};
  }
  return std::nullopt;
}

template <auto = 0>
static awaitable<void> send_welcome_chunks(boost::beast::tcp_stream& stream, std::string_view uuid, std::string_view nick) {
  auto ts = web_irc::utc_timestamp();
  auto page_begin = std::format("{}{}",
                                web_irc::gen::page_begin{.title = "Web IRC"},
                                web_irc::gen::channel{.id = "status", .topic = {}, .form_placeholder = "Message", .connection_id = uuid});

  co_await web_irc::send_stream_chunk(stream, page_begin);

  auto [... content] = std::tuple{"0.1.0",
                                  "Copyright (C) 2026 j4niwzis",
                                  "https://github.com/j4niwzis/web_irc",
                                  "Licensed under the GNU Affero General Public License, Version 3."};
  co_await web_irc::send_stream_chunk(
      stream,
      std::format("{}{}{}{}",
                  web_irc::gen::message{.channel = "status", .timestamp = ts, .author = "web-irc", .content = content}...,
                  web_irc::gen::message{
                      .channel = "status", .timestamp = ts, .author = "*", .content = std::format("Connecting to IRC as {}...", nick)}));
}

template <auto = 0>
awaitable<void> stream_main_page(asio::io_context& io,
                                 std::unordered_map<std::string, connection_entry>& connections,
                                 int& next_nicid,
                                 boost::beast::tcp_stream& stream,
                                 unsigned version) {
  auto uuid = generate_uuid();
  auto nick = std::format("WebClient{}", next_nicid++);

  web_irc::irc_config cfg;
  cfg.nickname = nick;
  cfg.username = nick;
  cfg.realname = "Web IRC Client";

  auto irc = std::make_shared<web_irc::irc_client>(io, std::move(cfg));
  irc->set_connection_id(uuid);
  connections[uuid] = {irc};

  auto ex = co_await asio::this_coro::executor;
  asio::co_spawn(ex, irc->connect(), [](std::exception_ptr ep) {
    if(ep) {
      try {
        std::rethrow_exception(ep);
      } catch(std::exception const& e) {
        std::println(stderr, "[irc] connection error: {}", e.what());
      }
    }
  });

  auto sub = irc->subscribe();

  std::exception_ptr ep;
  try {
    http::response<http::empty_body> res{http::status::ok, version};
    res.set(http::field::server, "web-irc");
    res.set(http::field::content_type, "text/html; charset=utf-8");
    res.chunked(true);

    http::response_serializer<http::empty_body> sr{res};
    co_await http::async_write_header(stream, sr, asio::use_awaitable);

    co_await send_welcome_chunks(stream, uuid, nick);

    try {
      while(true) {
        auto chunk = co_await irc->next_chunk(sub);
        if(chunk.empty()) {
          if(!irc->is_connected())
            break;
          co_await web_irc::send_stream_chunk(stream, std::string_view(" "));
        } else {
          co_await web_irc::send_stream_chunk(stream, chunk);
        }
      }
    } catch(...) {
      // client disconnected
    }
  } catch(...) {
    ep = std::current_exception();
  }

  irc->unsubscribe(sub);
  irc->close();
  connections.erase(uuid);
  try {
    co_await web_irc::send_stream_last_chunk(stream);
  } catch(...) {
    // ignore errors during teardown
  }
  if(ep)
    std::rethrow_exception(ep);
}

static awaitable<void> execute_slash_cmd(web_irc::irc_client& irc,
                                         std::string_view cmd,
                                         std::string_view args,
                                         const std::string& irc_channel) {
  if(cmd == "join") {
    auto ch = ensure_hash_prefix(trim_left(args));
    if(!ch.empty())
      co_await irc.join_channel(ch);
  } else if(cmd == "part") {
    auto rest = std::string_view(args);
    auto ch_end = rest.find(' ');
    auto ch_sv = ch_end == std::string_view::npos ? rest : rest.substr(0, ch_end);
    auto reason = ch_end == std::string_view::npos ? std::string_view{} : rest.substr(ch_end + 1);
    auto ch = ensure_hash_prefix(ch_sv.empty() ? irc_channel : ch_sv);
    if(!ch.empty())
      co_await irc.part_channel(ch, std::string(reason));
  } else if(cmd == "nick") {
    auto new_nick = trim_left(args);
    if(!new_nick.empty())
      co_await irc.change_nick(std::string(new_nick));
  } else if(cmd == "me") {
    if(irc_channel.empty())
      co_return;
    co_await irc.send_action(irc_channel, args);
    irc.inject_message(irc_channel, "*", std::format("{} {}", irc.nickname(), args));
  } else if(cmd == "msg") {
    auto rest = std::string_view(args);
    auto target_end = rest.find(' ');
    if(target_end == std::string_view::npos)
      co_return;
    auto target = rest.substr(0, target_end);
    auto msg_text = rest.substr(target_end + 1);
    if(target.empty())
      co_return;
    co_await irc.send_message(target, msg_text);
    irc.inject_message("status", irc.nickname(), std::format("[-> {}] {}", target, msg_text));
  } else if(cmd == "raw") {
    co_await irc.send_raw(std::string(args));
  } else if(cmd == "quit") {
    co_await irc.quit(std::string(args));
    irc.inject_message("status", "*", "You have quit IRC");
    irc.close();
  } else {
    irc.inject_message("status", "*", std::format("Unknown command: /{}", cmd));
  }
}

static awaitable<void> send_form_response(boost::beast::tcp_stream& stream,
                                          unsigned version,
                                          std::string_view channel,
                                          std::string_view connection_id) {
  auto placeholder = form_placeholder(channel);
  http::response<http::string_body> res{http::status::ok, version};
  res.set(http::field::server, "web-irc");
  res.set(http::field::content_type, "text/html; charset=utf-8");
  res.set(http::field::cache_control, "no-store, no-cache, must-revalidate");
  res.keep_alive(false);
  res.body() = std::format("{}", web_irc::gen::form_page{.channel = channel, .placeholder = placeholder, .connection_id = connection_id});
  res.prepare_payload();
  co_await web_irc::send_message(stream, std::move(res));
}

static awaitable<void> handle_send_message(std::unordered_map<std::string, connection_entry>& connections,
                                           std::string_view path,
                                           std::string_view body,
                                           boost::beast::tcp_stream& stream,
                                           unsigned version) {
  auto query = path;
  if(auto qpos = query.find('?'); qpos != std::string_view::npos)
    query.remove_prefix(qpos + 1);
  auto channel = extract_value(query, "channel");

  auto connection_id = extract_value(body, "connection_id");
  auto text = extract_value(body, "message");

  auto it = connections.find(connection_id);
  if(it != connections.end() && !text.empty()) {
    auto irc = it->second.irc;
    auto irc_channel = channel.empty() || channel[0] == '#' ? std::string(channel) : std::format("#{}", channel);

    if(text[0] == '/') {
      constexpr auto cmd_pattern = ctll::fixed_string{R"(/(\w+)\s*(.*))"};
      std::string_view cmd;
      std::string_view args;
      if(auto m = ctre::match<cmd_pattern>(std::string_view(text))) {
        cmd = m.template get<1>().to_view();
        args = m.template get<2>().to_view();
      }

      if(!cmd.empty()) {
        co_await execute_slash_cmd(*irc, cmd, args, irc_channel);
      }
    } else {
      if(!irc_channel.empty()) {
        co_await irc->send_message(irc_channel, text);
        irc->inject_message(irc_channel, irc->nickname(), text);
      }
    }
  }

  auto redirect_url = std::format("/form?channel={}&connection_id={}", channel, connection_id);
  co_await web_irc::send_redirect(stream, version, redirect_url);
}

static awaitable<void> serve_static_file(boost::beast::tcp_stream& stream, unsigned version, std::string_view path) {
  auto file_name = path.substr(8);
  if(file_name.empty() || file_name.find("..") != std::string_view::npos || file_name.find('/') != std::string_view::npos ||
     file_name.find('\\') != std::string_view::npos) {
    co_await web_irc::send_not_found(stream, version);
    co_return;
  }

  static constexpr std::pair<std::string_view, std::string_view> files[] = {
      {"client.css", client_css}, {"shadow.css", shadow_css}, {"page.css", page_css}};

  auto it = std::ranges::find(files, file_name, &std::pair<std::string_view, std::string_view>::first);
  if(it == std::end(files)) {
    co_await web_irc::send_not_found(stream, version);
    co_return;
  }

  auto content = it->second;
  std::vector<std::uint8_t> data(content.begin(), content.end());
  co_await web_irc::send_binary_response(stream, version, http::status::ok, "text/css", std::move(data), false);
}

awaitable<void> handle_request(asio::io_context& io,
                               std::unordered_map<std::string, connection_entry>& connections,
                               int& next_nicid,
                               boost::beast::tcp_stream& stream,
                               web_irc::request_t req) {
  auto path = std::string_view(req.target());
  auto version = req.version();

  if(req.method() == http::verb::get && path == "/") {
    co_await stream_main_page(io, connections, next_nicid, stream, version);
    co_return;
  }

  if(req.method() == http::verb::post && path.starts_with("/send_message")) {
    co_await handle_send_message(connections, path, req.body(), stream, version);
    co_return;
  }

  if(req.method() == http::verb::get && path.starts_with("/form")) {
    auto query = std::string_view(path);
    if(auto qpos = query.find('?'); qpos != std::string_view::npos)
      query.remove_prefix(qpos + 1);
    auto channel = extract_value(query, "channel");
    auto connection_id = extract_value(query, "connection_id");
    co_await send_form_response(stream, version, channel, connection_id);
    co_return;
  }

  if(req.method() == http::verb::get && path.starts_with("/static/")) {
    co_await serve_static_file(stream, version, path);
    co_return;
  }

  co_await web_irc::send_not_found(stream, version);
}

int main() {
  try {
    asio::io_context io(1);
    std::unordered_map<std::string, connection_entry> connections;
    int next_nicid = 1;

    web_irc::server_config cfg;

    auto handler = [&](boost::beast::tcp_stream& stream, web_irc::request_t req) -> awaitable<void> {
      co_await handle_request(io, connections, next_nicid, stream, std::move(req));
    };

    asio::co_spawn(io, web_irc::listener(handler, cfg), [](std::exception_ptr ep) {
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

module;
#include <cstdio>
export module web_irc.http_handlers;
import std;
import boost;
import ctre;
import web_irc.core;
import web_irc.gen;
import web_irc.irc;
import web_irc.utils;
import web_irc.static_assets;
import web_irc.slash_commands;
import web_irc.config;


namespace web_irc {

namespace http = boost::beast::http;
namespace asio = boost::asio;
using asio::awaitable;

export struct connection_entry {
  std::shared_ptr<irc_client> irc;
};

export template <auto = 0>
awaitable<void> send_welcome_chunks(boost::beast::tcp_stream& stream, std::string_view uuid, std::string_view nick) {
  auto ts = utc_timestamp();
  auto page_begin = std::format("{}{}",
                                gen::page_begin{.title = config.name, .connection_id = uuid},
                                gen::channel{.ref = gen::status_channel{}, .topic = std::nullopt, .form_placeholder = "", .connection_id = uuid});
  co_await send_stream_chunk(stream, page_begin);
  auto [... content] = std::tuple{config.version, config.copyright, config.url,
                                  "Licensed under the GNU Affero General Public License, Version 3."};
  co_await send_stream_chunk(
      stream,
      std::format("{}{}{}{}",
                  gen::message{.ref = gen::status_channel{}, .timestamp = ts, .author = config.short_name, .content = content, .author_class = ""}...,
                  gen::message{
                      .ref = gen::status_channel{}, .timestamp = ts, .author = "*", .content = std::format("Connecting to IRC as {}...", nick), .author_class = ""}));
}
export awaitable<void> stream_main_page(asio::io_context& io,
                                        std::unordered_map<std::string, connection_entry>& connections,
                                        boost::beast::tcp_stream& stream,
                                        unsigned version,
                                        irc_config cfg) {
  auto uuid = generate_uuid();
  auto nick = cfg.nickname;
  auto irc = std::make_shared<irc_client>(io, std::move(cfg));
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
    res.set(http::field::server, config.short_name);
    res.set(http::field::content_type, "text/html; charset=utf-8");
    res.chunked(true);
    http::response_serializer<http::empty_body> sr{res};
    co_await http::async_write_header(stream, sr, asio::use_awaitable);
    co_await send_welcome_chunks(stream, uuid, nick);
    try {
      while(true) {
        auto chunk = co_await irc->next_chunk(sub);
        if(chunk.empty()) {
          if(!irc->is_connected()) break;
          co_await send_stream_chunk(stream, std::string_view(" "));
        } else {
          co_await send_stream_chunk(stream, chunk);
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
    co_await send_stream_last_chunk(stream);
  } catch(...) {
    // ignore errors during teardown
  }
  if(ep) std::rethrow_exception(ep);
}
export awaitable<void> send_form_response(boost::beast::tcp_stream& stream,
                                           unsigned version,
                                           std::string_view channel,
                                           std::string_view connection_id) {
  auto placeholder = form_placeholder(channel);
  http::response<http::string_body> res{http::status::ok, version};
  res.set(http::field::server, config.short_name);
  res.set(http::field::content_type, "text/html; charset=utf-8");
  res.set(http::field::cache_control, "no-store, no-cache, must-revalidate");
  res.keep_alive(false);
  res.body() = std::format("{}", gen::submit_form{.ref = gen::regular_channel{channel}, .placeholder = placeholder, .connection_id = connection_id});
  res.prepare_payload();
  co_await send_message(stream, std::move(res));
}
export awaitable<void> send_status_form_response(boost::beast::tcp_stream& stream,
                                                  unsigned version,
                                                  std::string_view connection_id) {
  http::response<http::string_body> res{http::status::ok, version};
  res.set(http::field::server, config.short_name);
  res.set(http::field::content_type, "text/html; charset=utf-8");
  res.set(http::field::cache_control, "no-store, no-cache, must-revalidate");
  res.keep_alive(false);
  res.body() = std::format("{}", gen::submit_form{.ref = gen::status_channel{}, .placeholder = "Message", .connection_id = connection_id});
  res.prepare_payload();
  co_await send_message(stream, std::move(res));
}
export awaitable<void> handle_send_message(std::unordered_map<std::string, connection_entry>& connections,
                                           std::string_view path,
                                           std::string_view body,
                                           boost::beast::tcp_stream& stream,
                                           unsigned version) {
  auto query = path;
  if(auto qpos = query.find('?'); qpos != std::string_view::npos) query.remove_prefix(qpos + 1);
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
  auto redirect_url = std::format("/submit_form?channel={}&connection_id={}", channel, connection_id);
  co_await send_redirect(stream, version, redirect_url);
}
export awaitable<void> handle_send_status_message(std::unordered_map<std::string, connection_entry>& connections,
                                                   std::string_view body,
                                                   boost::beast::tcp_stream& stream,
                                                   unsigned version) {
  auto connection_id = extract_value(body, "connection_id");
  auto text = extract_value(body, "message");
  auto it = connections.find(connection_id);
  if(it != connections.end() && !text.empty()) {
    auto irc = it->second.irc;
    if(text[0] == '/') {
      constexpr auto cmd_pattern = ctll::fixed_string{R"(/(\w+)\s*(.*))"};
      std::string_view cmd;
      std::string_view args;
      if(auto m = ctre::match<cmd_pattern>(std::string_view(text))) {
        cmd = m.template get<1>().to_view();
        args = m.template get<2>().to_view();
      }
      if(!cmd.empty()) {
        co_await execute_slash_cmd(*irc, cmd, args, "");
      }
    } else {
      irc->inject_status_message(irc->nickname(), text);
    }
  }
  co_await send_redirect(stream, version, std::format("/submit_status_form?connection_id={}", connection_id));
}
export awaitable<void> handle_request(asio::io_context& io,
                                      std::unordered_map<std::string, connection_entry>& connections,
                                      int& next_nicid,
                                      boost::beast::tcp_stream& stream,
                                      request_t req) {
  auto path = std::string_view(req.target());
  auto version = req.version();
  if(req.method() == http::verb::get && path == "/") {
    co_await send_html_response(stream, version, http::status::ok, std::string(gen::html::landing_page), false);
    co_return;
  }
  if(req.method() == http::verb::get && path == "/connect_form") {
    auto nick = std::format("WebClient{}", next_nicid++);
    http::response<http::string_body> res{http::status::ok, version};
    res.set(http::field::server, config.short_name);
    res.set(http::field::content_type, "text/html; charset=utf-8");
    res.set(http::field::cache_control, "no-store, no-cache, must-revalidate");
    res.keep_alive(false);
    res.body() = std::format("{}", gen::connect_form{.default_nick = nick, .default_channels = config.default_channels});
    res.prepare_payload();
    co_await send_message(stream, std::move(res));
    co_return;
  }
  if(req.method() == http::verb::post && path == "/stream") {
    auto nick = extract_value(req.body(), "nick");
    auto channels_raw = extract_value(req.body(), "channels");
    if(nick.empty()) {
      co_await send_text_response(stream, version, http::status::bad_request, "missing nick");
      co_return;
    }
    irc_config cfg{.server = static_cast<std::string>(config.irc_server.address),
                   .port = config.irc_server.port,
                   .nickname = std::move(nick),
                   .realname = static_cast<std::string>(config.realname),
                   .channels = parse_channels(channels_raw)};
    cfg.username = cfg.nickname;
    if constexpr(std::holds_alternative<web_irc::webirc::use>(config.webirc)) {
      constexpr auto const& w = std::get<web_irc::webirc::use>(config.webirc);
      if constexpr(w.real_ip_from_headers) {
        auto forwarded = req["X-Forwarded-For"];
        if(!forwarded.empty()) {
          auto comma = forwarded.find(',');
          cfg.real_ip = std::string(forwarded.substr(0, comma));
        } else {
          auto real_ip = req["X-Real-IP"];
          cfg.real_ip = std::string(real_ip);
        }
      } else {
        auto remote = stream.socket().remote_endpoint();
        cfg.real_ip = remote.address().to_string();
      }
    }
    co_await stream_main_page(io, connections, stream, version, std::move(cfg));
    co_return;
  }
  if(req.method() == http::verb::post && path == "/send_status_message") {
    co_await handle_send_status_message(connections, req.body(), stream, version);
    co_return;
  }
  if(req.method() == http::verb::get && path.starts_with("/submit_status_form")) {
    auto query = std::string_view(path);
    if(auto qpos = query.find('?'); qpos != std::string_view::npos) query.remove_prefix(qpos + 1);
    auto connection_id = extract_value(query, "connection_id");
    co_await send_status_form_response(stream, version, connection_id);
    co_return;
  }
  if(req.method() == http::verb::post && path.starts_with("/send_message")) {
    co_await handle_send_message(connections, path, req.body(), stream, version);
    co_return;
  }
  if(req.method() == http::verb::get && path.starts_with("/submit_form")) {
    auto query = std::string_view(path);
    if(auto qpos = query.find('?'); qpos != std::string_view::npos) query.remove_prefix(qpos + 1);
    auto channel = extract_value(query, "channel");
    auto connection_id = extract_value(query, "connection_id");
    co_await send_form_response(stream, version, channel, connection_id);
    co_return;
  }
  if(req.method() == http::verb::get && path.starts_with("/connection_status")) {
    auto query = std::string_view(path);
    if(auto qpos = query.find('?'); qpos != std::string_view::npos) query.remove_prefix(qpos + 1);
    auto connection_id = extract_value(query, "connection_id");
    auto it = connections.find(connection_id);
    bool connected = it != connections.end() && it->second.irc && it->second.irc->is_connected();
    co_await send_html_response(
        stream,
        version,
        http::status::ok,
        std::string(connected ? gen::html::connection_status_ok : gen::html::connection_status_err),
        false);
    co_return;
  }
  if(req.method() == http::verb::get && path.starts_with("/static/")) {
    co_await serve_static_file(stream, version, path);
    co_return;
  }
  co_await send_not_found(stream, version);
}
} // namespace web_irc

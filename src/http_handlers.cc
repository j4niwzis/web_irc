module;
#include <cstdio>
export module web_irc.http_handlers;
import std;
import boost;
import ctre;
import mirc;
import web_irc.core;
import web_irc.render;
import web_irc.web_client;
import web_irc.utils.string_ops;
import web_irc.utils.uuid;
import web_irc.utils.channel_id;
import web_irc.static_assets;
import web_irc.slash_commands;
import web_irc.config;
import web_irc.render.templates.html;

namespace web_irc {

namespace http = boost::beast::http;
namespace asio = boost::asio;
namespace beast = boost::beast;
using asio::awaitable;

using utils::channel_id;
using utils::extract_value;
using utils::form_placeholder;
using utils::generate_uuid;
using utils::parse_channels;

export struct connection_entry {
  std::shared_ptr<web_client> irc;
};

export awaitable<void> stream_main_page(asio::io_context& io,                                            // NOLINT
                                        std::unordered_map<std::string, connection_entry>& connections,  // NOLINT
                                        beast::tcp_stream& stream,                                       // NOLINT
                                        unsigned version,
                                        web_client::config cfg,
                                        std::vector<std::string> channels) {
  auto uuid = generate_uuid();
  auto nick = cfg.irc_config.nickname;
  auto irc = std::make_shared<web_client>(uuid);

  connections[uuid] = {irc};
  auto ex = co_await asio::this_coro::executor;

  co_await irc->connect(std::move(cfg));
  co_await irc->send(channels | std::views::transform([](std::string_view str) {
                       return mirc::client::join{.channel = str};
                     }));
  auto sub = co_await irc->subscribe();

  asio::co_spawn(ex, irc->run(), [](std::exception_ptr ep) {
    if(ep) {
      try {
        std::rethrow_exception(ep);
      } catch(const std::exception& e) {
        std::println(stderr, "[irc] connection error: {}", e.what());
      }
    }
  });
  std::exception_ptr ep;
  try {
    http::response<http::empty_body> res{http::status::ok, version};
    res.set(http::field::server, config.short_name);
    res.set(http::field::content_type, "text/html; charset=utf-8");
    res.chunked(true);
    http::response_serializer<http::empty_body> sr{res};
    co_await http::async_write_header(stream, sr, asio::use_awaitable);

    try {
      for(;;) {
        auto chunk = co_await sub->async_receive(asio::use_awaitable);
        co_await send_stream_chunk(stream, chunk);
      }
    } catch(...) {
      // client disconnected
    }
  } catch(...) {
    ep = std::current_exception();
  }

  co_await irc->unsubscribe(sub);
  co_await irc->close();

  connections.erase(uuid);
  try {
    co_await send_stream_last_chunk(stream);
  } catch(...) {
    // ignore errors during teardown
  }
  if(ep) {
    std::rethrow_exception(ep);
  }
}
export awaitable<void> send_form_response(beast::tcp_stream& stream,  // NOLINT
                                          unsigned version,
                                          std::string_view channel,
                                          std::string_view connection_id) {
  auto placeholder = form_placeholder(channel);
  http::response<http::string_body> res{http::status::ok, version};
  res.set(http::field::server, config.short_name);
  res.set(http::field::content_type, "text/html; charset=utf-8");
  res.set(http::field::cache_control, "no-store, no-cache, must-revalidate");
  res.keep_alive(false);
  res.body() = std::format(
      "{}", render::submit_form{.channel = render::regular_channel{channel}, .placeholder = placeholder, .connection_id = connection_id});
  res.prepare_payload();
  co_await send_message(stream, std::move(res));
}
// NOLINTNEXTLINE
export awaitable<void> send_status_form_response(beast::tcp_stream& stream, unsigned version, std::string_view connection_id) {
  http::response<http::string_body> res{http::status::ok, version};
  res.set(http::field::server, config.short_name);
  res.set(http::field::content_type, "text/html; charset=utf-8");
  res.set(http::field::cache_control, "no-store, no-cache, must-revalidate");
  res.keep_alive(false);
  res.body() =
      std::format("{}", render::submit_form{.channel = render::status_channel{}, .placeholder = "Message", .connection_id = connection_id});
  res.prepare_payload();
  co_await send_message(stream, std::move(res));
}
export awaitable<void> handle_send_message(std::unordered_map<std::string, connection_entry>& connections,  // NOLINT
                                           std::string_view path,
                                           std::string_view body,
                                           boost::beast::tcp_stream& stream,  // NOLINT
                                           unsigned version) {
  auto query = path;
  if(auto qpos = query.find('?'); qpos != std::string_view::npos) {
    query.remove_prefix(qpos + 1);
  }
  auto channel = extract_value(query, "channel");
  auto connection_id = extract_value(body, "connection_id");
  auto text = extract_value(body, "message");
  auto it = connections.find(connection_id);
  if(it != connections.end() && !text.empty()) {
    auto irc = it->second.irc;
    auto irc_channel = channel.empty() || channel[0] == '#' ? std::string(channel) : std::format("#{}", channel);
    if(text[0] == '/') {
      auto cmd = parse_slash_cmd(text, irc_channel);
      if(cmd) {
        co_await execute_slash_cmd(*irc, *cmd);
      }
    } else {
      if(!irc_channel.empty()) {
        co_await irc->send(mirc::client::priv_msg{.target = irc_channel, .text = text});
        irc->render().message(channel_id(irc_channel), irc->nickname(), text).send();
        co_await irc->commit();
      }
    }
  }
  auto redirect_url = std::format("/submit_form?channel={}&connection_id={}", channel, connection_id);
  co_await send_redirect(stream, version, redirect_url);
}
export awaitable<void> handle_send_status_message(std::unordered_map<std::string, connection_entry>& connections,  // NOLINT
                                                  std::string_view body,
                                                  beast::tcp_stream& stream,  // NOLINT
                                                  unsigned version) {
  auto connection_id = extract_value(body, "connection_id");
  auto text = extract_value(body, "message");
  auto it = connections.find(connection_id);
  if(it != connections.end() && !text.empty()) {
    auto irc = it->second.irc;
    if(text[0] == '/') {
      auto cmd = parse_slash_cmd(text, "");
      if(cmd) {
        co_await execute_slash_cmd(*irc, *cmd);
      }
    } else {
      irc->render().status_message(irc->nickname(), text.subview()).send();
      co_await irc->commit();
    }
  }
  co_await send_redirect(stream, version, std::format("/submit_status_form?connection_id={}", connection_id));
}
export awaitable<void> handle_request(asio::io_context& io,                                            // NOLINT
                                      std::unordered_map<std::string, connection_entry>& connections,  // NOLINT
                                      int& next_nicid,                                                 // NOLINT
                                      beast::tcp_stream& stream,                                       // NOLINT
                                      request_t req) {
  auto path = std::string_view(req.target());
  auto version = req.version();
  if(req.method() == http::verb::get && path == "/") {
    co_await send_html_response(stream, version, http::status::ok, std::string(render::html::landing_page), false);
    co_return;
  }
  if(req.method() == http::verb::get && path == "/connect_form") {
    auto nick = std::format("WebClient{}", next_nicid++);
    http::response<http::string_body> res{http::status::ok, version};
    res.set(http::field::server, config.short_name);
    res.set(http::field::content_type, "text/html; charset=utf-8");
    res.set(http::field::cache_control, "no-store, no-cache, must-revalidate");
    res.keep_alive(false);
    res.body() = std::format("{}", render::connect_form{.default_nick = nick, .default_channels = config.default_channels});
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
    auto real_ip = [&] {
      if constexpr(std::holds_alternative<web_irc::webirc::use>(config.webirc)) {
        constexpr auto const& w = std::get<web_irc::webirc::use>(config.webirc);
        if constexpr(w.real_ip_from_headers) {
          auto forwarded = req["X-Forwarded-For"];
          if(!forwarded.empty()) {
            auto comma = forwarded.find(',');
            return std::string(forwarded.substr(0, comma));
          } else {
            auto real_ip = req["X-Real-IP"];
            return std::string(real_ip);
          }
        } else {
          auto remote = stream.socket().remote_endpoint();
          return remote.address().to_string();
        }
      }
    }();
    web_client::config cfg{
        .server = static_cast<std::string>(config.irc_server.address),
        .port = config.irc_server.port,
        .irc_config = {.nickname = std::move(nick), .realname = static_cast<std::string>(config.realname), .real_ip = std::move(real_ip)}};
    cfg.irc_config.username = cfg.irc_config.nickname;

    co_await stream_main_page(io, connections, stream, version, std::move(cfg), parse_channels(channels_raw));
    co_return;
  }
  if(req.method() == http::verb::post && path == "/send_status_message") {
    co_await handle_send_status_message(connections, req.body(), stream, version);
    co_return;
  }
  if(req.method() == http::verb::get && path.starts_with("/submit_status_form")) {
    auto query = std::string_view(path);
    if(auto qpos = query.find('?'); qpos != std::string_view::npos) {
      query.remove_prefix(qpos + 1);
    }
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
    if(auto qpos = query.find('?'); qpos != std::string_view::npos) {
      query.remove_prefix(qpos + 1);
    }
    auto channel = extract_value(query, "channel");
    auto connection_id = extract_value(query, "connection_id");
    co_await send_form_response(stream, version, channel, connection_id);
    co_return;
  }
  if(req.method() == http::verb::get && path.starts_with("/connection_status")) {
    auto query = std::string_view(path);
    if(auto qpos = query.find('?'); qpos != std::string_view::npos) {
      query.remove_prefix(qpos + 1);
    }
    auto connection_id = extract_value(query, "connection_id");
    auto it = connections.find(connection_id);
    bool connected = it != connections.end() && it->second.irc && it->second.irc->is_running();
    co_await send_html_response(stream,
                                version,
                                http::status::ok,
                                std::string(connected ? render::html::connection_status_ok : render::html::connection_status_err),
                                false);
    co_return;
  }
  if(req.method() == http::verb::get && path.starts_with("/static/")) {
    co_await serve_static_file(stream, version, path);
    co_return;
  }
  co_await send_not_found(stream, version);
}

}  // namespace web_irc

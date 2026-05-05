export module web_irc.static_assets;
import std;
import web_irc.core;
import web_irc.config;


namespace web_irc {

namespace {
constexpr auto client_css = [] {
  static constexpr char data[] = {
#embed "../static/client.css"
  };
  return std::string_view(data, sizeof(data));
}();
constexpr auto shadow_css = [] {
  static constexpr char data[] = {
#embed "../static/shadow.css"
  };
  return std::string_view(data, sizeof(data));
}();
constexpr auto page_css = [] {
  static constexpr char data[] = {
#embed "../static/page.css"
  };
  return std::string_view(data, sizeof(data));
}();
constexpr auto connect_form_css = [] {
  static constexpr char data[] = {
#embed "../static/connect_form.css"
  };
  return std::string_view(data, sizeof(data));
}();
constexpr auto landing_page_css = [] {
  static constexpr char data[] = {
#embed "../static/landing_page.css"
  };
  return std::string_view(data, sizeof(data));
}();
constexpr auto beep_wav = [] {
  static constexpr char data[] = {
#embed "../static/beep.wav"
  };
  return std::string_view(data, sizeof(data));
}();
} // namespace
namespace beast = boost::beast;
namespace http = beast::http;
using boost::asio::awaitable;
export awaitable<void> serve_static_file(beast::tcp_stream& stream, unsigned version, std::string_view path) {
  auto file_name = path.substr(8);
  if(file_name.empty() || file_name.find("..") != std::string_view::npos || file_name.find('/') != std::string_view::npos ||
     file_name.find('\\') != std::string_view::npos) {
    co_await send_not_found(stream, version);
    co_return;
  }
  static constexpr std::tuple<std::string_view, std::string_view, std::string_view> files[] = {
      {"client.css", client_css, "text/css"},
      {"shadow.css", shadow_css, "text/css"},
      {"page.css", page_css, "text/css"},
      {"connect_form.css", connect_form_css, "text/css"},
      {"landing_page.css", landing_page_css, "text/css"},
      {"beep.wav", beep_wav, "audio/wav"}};
  auto it = std::ranges::find(files, file_name, [](const auto& t) { return std::get<0>(t); });
  if(it == std::end(files)) {
    co_await send_not_found(stream, version);
    co_return;
  }
  auto content = std::get<1>(*it);
  auto content_type = std::get<2>(*it);
  std::vector<std::uint8_t> data(content.begin(), content.end());
  co_await send_binary_response(stream, version, http::status::ok, content_type, std::move(data), false);
}
} // namespace web_irc

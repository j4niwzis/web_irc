export module web_irc.render.templates.html;
import std;

namespace web_irc::render::html {

export inline constexpr std::string_view page_end = [] -> std::string_view {
  // NOLINTNEXTLINE
  static constexpr const char html[] = {
#embed "../templates/page_end.html"
  };
  // NOLINTNEXTLINE
  return {html, sizeof(html)};
}();

export inline constexpr std::string_view landing_page = [] -> std::string_view {
  // NOLINTNEXTLINE
  static constexpr const char html[] = {
#embed "../templates/landing_page.html"
  };
  // NOLINTNEXTLINE
  return {html, sizeof(html)};
}();

export inline constexpr std::string_view connection_status_ok = [] -> std::string_view {
  // NOLINTNEXTLINE
  static constexpr const char html[] = {
#embed "../templates/connection_status_ok.html"
  };
  // NOLINTNEXTLINE
  return {html, sizeof(html)};
}();

export inline constexpr std::string_view connection_status_err = [] -> std::string_view {
  // NOLINTNEXTLINE
  static constexpr const char html[] = {
#embed "../templates/connection_status_err.html"
  };
  // NOLINTNEXTLINE
  return {html, sizeof(html)};
}();

export inline constexpr std::string_view beep = [] -> std::string_view {
  // NOLINTNEXTLINE
  static constexpr const char html[] = {
#embed "../templates/beep.html"
  };
  // NOLINTNEXTLINE
  return {html, sizeof(html)};
}();

}  // namespace web_irc::render::html

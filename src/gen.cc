export module web_irc.gen;
import std;
import web_irc.range_string_formatter;

namespace web_irc::gen {

constexpr auto to_array(auto... args) {
  return std::to_array<char>({static_cast<char>(args)...});
}

namespace html {

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

}  // namespace html

constexpr auto escape_html(std::string_view input) {
  return input | std::views::transform([](const char& c) -> std::string_view {
           switch(c) {
             case '&':
               return "&amp;";
             case '<':
               return "&lt;";
             case '>':
               return "&gt;";
             case '"':
               return "&quot;";
             case '\'':
               return "&#39;";
             default:
               return std::string_view{&c, 1};
           }
         }) |
         std::views::join;
}

namespace fmt {

constexpr auto channel_rules = to_array(
#embed "../templates/channel_rules.css.fmt"
    , 0);

constexpr auto page_begin = to_array(
#embed "../templates/page.html.fmt"
    , 0);

constexpr auto tab = to_array(
#embed "../templates/tab.html.fmt"
    , 0);

constexpr auto topic = to_array(
#embed "../templates/topic.html.fmt"
    , 0);

constexpr auto message = to_array(
#embed "../templates/message.html.fmt"
    , 0);

constexpr auto user = to_array(
#embed "../templates/user.html.fmt"
    , 0);

constexpr auto user_hide = to_array(
#embed "../templates/user_hide.html.fmt"
    , 0);

constexpr auto user_show = to_array(
#embed "../templates/user_show.html.fmt"
    , 0);

constexpr auto submit_form = to_array(
#embed "../templates/submit_form.html.fmt"
    , 0);

constexpr auto connect_form = to_array(
#embed "../templates/connect_form.html.fmt"
    , 0);

constexpr auto composer_frame = to_array(
#embed "../templates/composer_frame.html.fmt"
    , 0);

constexpr auto bottom_anchor = to_array(
#embed "../templates/bottom_anchor.html.fmt"
    , 0);

}  // namespace fmt

export struct bottom_anchor {
  std::string_view id;
};

export struct page_begin {
  std::string_view title;
  std::string_view connection_id;
};

export struct channel_rules {
  std::string_view channel_id;
};

export struct tab {
  std::string_view id;
  bool show_hash = true;
};

export struct topic {
  std::string_view channel;
  std::string_view content;
};

export struct message {
  std::string_view channel;
  std::string_view timestamp;
  std::string_view author;
  std::string_view content;
};

export struct user {
  std::string_view channel;
  std::string_view user;
};

export struct user_hide {
  std::string_view channel;
  std::string_view user;
};

export struct user_show {
  std::string_view channel;
  std::string_view user;
};

export struct submit_form {
  std::string_view channel;
  std::string_view placeholder;
  std::string_view connection_id;
};

export struct composer_frame {
  std::string_view channel;
  std::string_view placeholder;
  std::string_view connection_id;
};
export struct connect_form {
  std::string_view default_nick;
  std::string_view default_channels;
};

export struct channel {
  std::string_view id;
  std::optional<std::string_view> topic;
  std::string_view form_placeholder;
  std::string_view connection_id;
};

template <typename T, auto& fmt, typename Char>
struct formatter_helper {
  [[nodiscard]] constexpr auto format(T obj, auto& ctx) const {
    auto [... vs] = obj;
    return std::format_to(ctx.out(), fmt.data(), range_string_formatter{escape_html(vs)}...);
  }
  [[nodiscard]] constexpr auto parse(auto& ctx) const {
    return ctx.begin();
  }
};

}  // namespace web_irc::gen
using namespace web_irc::gen;  // NOLINT

export template <typename Char>
struct std::formatter<page_begin, Char> : formatter_helper<page_begin, web_irc::gen::fmt::page_begin, Char> {};

export template <typename Char>
struct std::formatter<channel_rules, Char> : formatter_helper<channel_rules, web_irc::gen::fmt::channel_rules, Char> {};

export template <typename Char>
struct std::formatter<tab, Char> {
  [[nodiscard]] constexpr auto format(tab t, auto& ctx) const {
    auto prefix = t.show_hash ? "#" : "";
    return std::format_to(ctx.out(), web_irc::gen::fmt::tab.data(), web_irc::range_string_formatter{escape_html(t.id)}, prefix);
  }
  [[nodiscard]] constexpr auto parse(auto& ctx) const {
    return ctx.begin();
  }
};

export template <typename Char>
struct std::formatter<topic, Char> : formatter_helper<topic, web_irc::gen::fmt::topic, Char> {};

export template <typename Char>
struct std::formatter<message, Char> : formatter_helper<message, web_irc::gen::fmt::message, Char> {};

export template <typename Char>
struct std::formatter<user, Char> : formatter_helper<user, web_irc::gen::fmt::user, Char> {};

export template <typename Char>
struct std::formatter<user_hide, Char> : formatter_helper<user_hide, web_irc::gen::fmt::user_hide, Char> {};

export template <typename Char>
struct std::formatter<user_show, Char> : formatter_helper<user_show, web_irc::gen::fmt::user_show, Char> {};

export template <typename Char>
struct std::formatter<connect_form, Char> : formatter_helper<connect_form, web_irc::gen::fmt::connect_form, Char> {};

export template <typename Char>
struct std::formatter<submit_form, Char> : formatter_helper<submit_form, web_irc::gen::fmt::submit_form, Char> {};

export template <typename Char>
struct std::formatter<composer_frame, Char> : formatter_helper<composer_frame, web_irc::gen::fmt::composer_frame, Char> {};

export template <typename Char>
struct std::formatter<bottom_anchor, Char> : formatter_helper<bottom_anchor, web_irc::gen::fmt::bottom_anchor, Char> {};

export template <typename Char>
struct std::formatter<channel, Char> : std::formatter<std::string_view> {
  [[nodiscard]] constexpr auto format(channel ch, auto& ctx) const {
    const auto rules = channel_rules{.channel_id = ch.id};
    const auto t = tab{.id = ch.id, .show_hash = ch.id != "status"};
    const auto anchor = bottom_anchor{.id = ch.id};
    const auto frame = composer_frame{.channel = ch.id, .placeholder = ch.form_placeholder, .connection_id = ch.connection_id};

    return ch.topic ? std::format_to(ctx.out(), "{}{}{}{}{}", rules, t, anchor, topic{.channel = ch.id, .content = *ch.topic}, frame)
                    : std::format_to(ctx.out(), "{}{}{}{}", rules, t, anchor, frame);
  }
};

export template struct std::formatter<page_begin, char>;
export template struct std::formatter<channel_rules, char>;
export template struct std::formatter<tab, char>;
export template struct std::formatter<topic, char>;
export template struct std::formatter<message, char>;
export template struct std::formatter<user, char>;
export template struct std::formatter<submit_form, char>;
export template struct std::formatter<connect_form, char>;
export template struct std::formatter<composer_frame, char>;
export template struct std::formatter<bottom_anchor, char>;
export template struct std::formatter<channel, char>;

export template struct std::formatter<user_hide, char>;
export template struct std::formatter<user_show, char>;

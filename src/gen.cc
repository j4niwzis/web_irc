export module web_irc.gen;
import std;

namespace web_irc::gen {

namespace html {

export inline constexpr std::string_view page_end = [] -> std::string_view {
  // NOLINTNEXTLINE
  static constexpr const char html[] = {
#embed "../templates/page_end.html"
  };
  // NOLINTNEXTLINE
  return {html, sizeof(html)};
}();

}  // namespace html

namespace fmt {

constexpr auto channel_rules = std::to_array<char>({
#embed "../templates/channel_rules.css.fmt"
    , 0});

constexpr auto page_begin = std::to_array<char>({// NOLINT
#embed "../templates/page.html.fmt"
                                                 ,
                                                 0});

constexpr auto tab = std::to_array<char>({// NOLINT
#embed "../templates/tab.html.fmt"
                                          ,
                                          0});

constexpr auto topic = std::to_array<char>({// NOLINT
#embed "../templates/topic.html.fmt"
                                            ,
                                            0});

constexpr auto message = std::to_array<char>({// NOLINT
#embed "../templates/message.html.fmt"
                                              ,
                                              0});

constexpr auto user = std::to_array<char>({// NOLINT
#embed "../templates/user.html.fmt"
                                           ,
                                           0});

constexpr auto user_hide = std::to_array<char>({// NOLINT
#embed "../templates/user_hide.html.fmt"
                                                ,
                                                0});

constexpr auto user_show = std::to_array<char>({// NOLINT
#embed "../templates/user_show.html.fmt"
                                                ,
                                                0});

constexpr auto form_page = std::to_array<char>({// NOLINT
#embed "../templates/form_page.html.fmt"
                                                ,
                                                0});

constexpr auto composer_frame = std::to_array<char>({// NOLINT
#embed "../templates/composer_frame.html.fmt"
                                                     ,
                                                     0});

constexpr auto bottom_anchor = std::to_array<char>({// NOLINT
#embed "../templates/bottom_anchor.html.fmt"
                                                    ,
                                                    0});

}  // namespace fmt

export struct bottom_anchor {
  std::string_view id;
};

export struct page_begin {
  std::string_view title;
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

export struct form_page {
  std::string_view channel;
  std::string_view placeholder;
  std::string_view connection_id;
};

export struct composer_frame {
  std::string_view channel;
  std::string_view placeholder;
  std::string_view connection_id;
};

export struct channel {
  std::string_view id;
  std::optional<std::string_view> topic;
  std::string_view form_placeholder;
  std::string_view connection_id;
};

template <typename T, auto fmt, typename Char>
struct formatter_helper {
  [[nodiscard]] constexpr auto format(T obj, auto& ctx) const {
    auto [... vs] = obj;
    return std::format_to(ctx.out(), fmt.data(), vs...);
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
    return std::format_to(ctx.out(), web_irc::gen::fmt::tab.data(), t.id, prefix);
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
struct std::formatter<form_page, Char> : formatter_helper<form_page, web_irc::gen::fmt::form_page, Char> {};

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
export template struct std::formatter<form_page, char>;
export template struct std::formatter<composer_frame, char>;
export template struct std::formatter<bottom_anchor, char>;
export template struct std::formatter<channel, char>;

export template struct std::formatter<user_hide, char>;
export template struct std::formatter<user_show, char>;

namespace {

inline void fix() {
  std::ignore = std::format("{}", tab{.id = ""});  // idk why but code can't be compiled without it
}

}  // namespace

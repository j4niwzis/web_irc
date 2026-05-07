export module web_irc.gen;
import std;
import ctre;
import web_irc.range_string_formatter;

namespace web_irc::gen {

template <typename... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template <typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

constexpr auto to_array(auto... args) {
  return std::to_array<char>({static_cast<char>(args)...});
}

template <std::size_t N>
struct static_string {
  char data[N];
  constexpr static_string(const char (&str)[N]) {
    for (std::size_t i = 0; i < N; ++i) data[i] = str[i];
  }
  constexpr std::size_t size() const { return N - 1; }
  constexpr const char* c_str() const { return data; }
};
template <std::size_t N>
static_string(const char (&str)[N]) -> static_string<N>;


template <static_string>
struct string_tag {};

export template <static_string fmt, typename... Ts>
struct lazy_format {
  constexpr lazy_format(string_tag<fmt>, const Ts&... args) : values{args...} {}
  std::tuple<const Ts&...> values;
};

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

template <typename OutIt>
struct escape_html_iterator {
  OutIt out;

  escape_html_iterator& operator=(char c) {
    switch(c) {
      case '&': out = std::format_to(out, "&amp;"); break;
      case '<': out = std::format_to(out, "&lt;"); break;
      case '>': out = std::format_to(out, "&gt;"); break;
      case '"': out = std::format_to(out, "&quot;"); break;
      case '\'': out = std::format_to(out, "&#39;"); break;
      default: *out = c; ++out; break;
    }
    return *this;
  }

  escape_html_iterator& operator*() { return *this; }
  escape_html_iterator& operator++() { return *this; }
  escape_html_iterator operator++(int) { return *this; }

  using difference_type = std::ptrdiff_t;
  using value_type = char;
  using pointer = void;
  using reference = void;
  using iterator_category = std::output_iterator_tag;
};

export template <typename T>
struct escape_html {
  const T& value;
};

export template <typename T>
escape_html(const T&) -> escape_html<T>;

export struct regular_channel {
  std::string_view id;
};

export struct status_channel {};

export using channel_ref = std::variant<regular_channel, status_channel>;

enum class channel_spec {
  none,
  tab_class,
  tab_href,
  tab_label,
  msg_class,
  users_class,
  frame_class,
  frame_src,
  jump_class,
  topic_class,
  form_action,
  anchor_id
};

export struct bottom_anchor {
  channel_ref ref;
};

export struct page_begin {
  std::string_view title;
  std::string_view connection_id;
};

export struct channel_rules {
  channel_ref ref;
};

export struct tab {
  channel_ref ref;
};

export struct topic {
  channel_ref ref;
  std::string_view content;
};

export struct message {
  channel_ref ref;
  std::string_view timestamp;
  std::string_view author;
  std::string_view content;
  std::string_view author_class;
};

export struct user {
  channel_ref ref;
  std::string_view user;
};

export struct user_hide {
  channel_ref ref;
  std::string_view user;
};

export struct user_show {
  channel_ref ref;
  std::string_view user;
};

export struct submit_form {
  channel_ref ref;
  std::string_view placeholder;
  std::string_view connection_id;
};

export struct composer_frame {
  channel_ref ref;
  std::string_view connection_id;
};

export struct connect_form {
  std::string_view default_nick;
  std::string_view default_channels;
};

export struct channel {
  channel_ref ref;
  std::optional<std::string_view> topic;
  std::string_view form_placeholder;
  std::string_view connection_id;
};

template <typename T, auto& fmt, typename Char>
struct formatter_helper {
  [[nodiscard]] constexpr auto format(T obj, auto& ctx) const {
    auto [... vs] = obj;
    return std::format_to(ctx.out(), fmt.data(), escape_html(vs)...);
  }
  [[nodiscard]] constexpr auto parse(auto& ctx) const {
    return ctx.begin();
  }
};

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

}  // namespace web_irc::gen
using namespace web_irc::gen;  // NOLINT

export template <typename T, typename Char> requires (std::same_as<std::string_view, T> || std::same_as<std::string, T>)
struct std::formatter<web_irc::gen::escape_html<T>, Char> {
  [[nodiscard]] constexpr auto parse(auto& ctx) const {
    return ctx.begin();
  }
  [[nodiscard]] constexpr auto format(const web_irc::gen::escape_html<T>& e, auto& ctx) const {
    auto it = web_irc::gen::escape_html_iterator{ctx.out()};
    auto result = std::format_to(it, "{}", e.value);
    return result.out;
  }
};

export template <std::size_t N, typename Char>
struct std::formatter<web_irc::gen::escape_html<char[N]>, Char> {
  [[nodiscard]] constexpr auto parse(auto& ctx) const {
    return ctx.begin();
  }
  [[nodiscard]] constexpr auto format(const web_irc::gen::escape_html<char[N]>& e, auto& ctx) const {
    auto it = web_irc::gen::escape_html_iterator{ctx.out()};
    auto result = std::format_to(it, "{}", e.value);
    return result.out;
  }
};

export template <typename T, typename Char>
struct std::formatter<web_irc::gen::escape_html<T>, Char> : std::formatter<T, Char> {
  constexpr formatter() = default;
  [[nodiscard]] constexpr auto parse(auto& ctx) {
    return static_cast<std::formatter<T, Char>&>(*this).parse(ctx);
  }
  [[nodiscard]] constexpr auto format(const web_irc::gen::escape_html<T>& e, auto& ctx) const {
    struct {
      web_irc::gen::escape_html_iterator<decltype(ctx.out())> it;
      auto& out() {
        return this->it;
      }
    } new_ctx{.it = web_irc::gen::escape_html_iterator{ctx.out()}};
    return static_cast<const std::formatter<T, Char>&>(*this).format(e.value, new_ctx).out;
  }
};

export template <typename Char>
struct std::formatter<web_irc::gen::channel_ref, Char> {
  web_irc::gen::channel_spec spec = web_irc::gen::channel_spec::none;

  constexpr auto parse(auto& ctx) {
    auto it = ctx.begin();
    auto end = ctx.end();
    if (it != end && *it != '}') {
      auto start = it;
      while (it != end && *it != '}') ++it;
      auto sv = std::string_view(start, it - start);
      if (sv == "tab_class") spec = web_irc::gen::channel_spec::tab_class;
      else if (sv == "tab_href") spec = web_irc::gen::channel_spec::tab_href;
      else if (sv == "tab_label") spec = web_irc::gen::channel_spec::tab_label;
      else if (sv == "msg_class") spec = web_irc::gen::channel_spec::msg_class;
      else if (sv == "users_class") spec = web_irc::gen::channel_spec::users_class;
      else if (sv == "frame_class") spec = web_irc::gen::channel_spec::frame_class;
      else if (sv == "frame_src") spec = web_irc::gen::channel_spec::frame_src;
      else if (sv == "jump_class") spec = web_irc::gen::channel_spec::jump_class;
      else if (sv == "topic_class") spec = web_irc::gen::channel_spec::topic_class;
      else if (sv == "form_action") spec = web_irc::gen::channel_spec::form_action;
      else if (sv == "anchor_id") spec = web_irc::gen::channel_spec::anchor_id;
    }
    return it;
  }

  constexpr auto format(web_irc::gen::channel_ref ref, auto& ctx) const {
    return std::visit(web_irc::gen::overloaded{
      [&](const web_irc::gen::regular_channel& ch) {
        switch (spec) {
          case web_irc::gen::channel_spec::tab_class: return std::format_to(ctx.out(), "tab-{}", ch.id);
          case web_irc::gen::channel_spec::tab_href: return std::format_to(ctx.out(), "#tab-{}", ch.id);
          case web_irc::gen::channel_spec::tab_label: return std::format_to(ctx.out(), "#{}", ch.id);
          case web_irc::gen::channel_spec::msg_class: return std::format_to(ctx.out(), "inject-item-page-element-{}", ch.id);
          case web_irc::gen::channel_spec::users_class: return std::format_to(ctx.out(), "inject-users-{}", ch.id);
          case web_irc::gen::channel_spec::frame_class: return std::format_to(ctx.out(), "inject-frame-{}", ch.id);
          case web_irc::gen::channel_spec::frame_src: return std::format_to(ctx.out(), "/submit_form?channel={}", ch.id);
          case web_irc::gen::channel_spec::jump_class: return std::format_to(ctx.out(), "jump-{}", ch.id);
          case web_irc::gen::channel_spec::topic_class: return std::format_to(ctx.out(), "page-{}", ch.id);
          case web_irc::gen::channel_spec::form_action: return std::format_to(ctx.out(), "/send_message?channel={}", ch.id);
          case web_irc::gen::channel_spec::anchor_id: return std::format_to(ctx.out(), "tab-{}", ch.id);
          default: return ctx.out();
        }
      },
      [&](const web_irc::gen::status_channel&) {
        switch (spec) {
          case web_irc::gen::channel_spec::tab_class: return std::format_to(ctx.out(), "status-tab");
          case web_irc::gen::channel_spec::tab_href: return std::format_to(ctx.out(), "#status-tab");
          case web_irc::gen::channel_spec::tab_label: return std::format_to(ctx.out(), "Status");
          case web_irc::gen::channel_spec::msg_class: return std::format_to(ctx.out(), "status-inject-item-page-element");
          case web_irc::gen::channel_spec::users_class: return std::format_to(ctx.out(), "status-users");
          case web_irc::gen::channel_spec::frame_class: return std::format_to(ctx.out(), "status-frame");
          case web_irc::gen::channel_spec::frame_src: return std::format_to(ctx.out(), "/submit_status_form");
          case web_irc::gen::channel_spec::jump_class: return std::format_to(ctx.out(), "status-jump");
          case web_irc::gen::channel_spec::topic_class: return std::format_to(ctx.out(), "status-topic");
          case web_irc::gen::channel_spec::form_action: return std::format_to(ctx.out(), "/send_status_message");
          case web_irc::gen::channel_spec::anchor_id: return std::format_to(ctx.out(), "status-tab");
          default: return ctx.out();
        }
      }
    }, ref);
  }
};



export template <static_string fmt, typename... Ts, typename Char>
struct std::formatter<web_irc::gen::lazy_format<fmt, Ts...>, Char> {
  constexpr auto parse(auto& ctx) const { return ctx.begin(); }
  constexpr auto format(const web_irc::gen::lazy_format<fmt, Ts...>& lf, auto& ctx) const {
    return std::apply([&](const auto&... args) {
      return std::format_to(ctx.out(), fmt.data, args...);
    }, lf.values);
  }
};

export template <typename Char>
struct std::formatter<page_begin, Char> : formatter_helper<page_begin, web_irc::gen::fmt::page_begin, Char> {};

export template <typename Char>
struct std::formatter<channel_rules, Char> : formatter_helper<channel_rules, web_irc::gen::fmt::channel_rules, Char> {};

export template <typename Char>
struct std::formatter<tab, Char> : formatter_helper<tab, web_irc::gen::fmt::tab, Char> {};

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
struct std::formatter<bottom_anchor, Char> : formatter_helper<bottom_anchor, web_irc::gen::fmt::bottom_anchor, Char> {};

export template <typename Char>
struct std::formatter<composer_frame, Char> {
  constexpr auto parse(auto& ctx) { return ctx.begin(); }
  constexpr auto format(composer_frame f, auto& ctx) const {
    return std::visit(web_irc::gen::overloaded{
      [&](const web_irc::gen::regular_channel& ch) {
      	auto escaped_id = escape_html(ch.id);
        auto escaped_connection_id = escape_html(f.connection_id);
	return std::format_to(ctx.out(), web_irc::gen::fmt::composer_frame.data(),
          web_irc::gen::lazy_format{string_tag<"inject-frame-{}">{}, escaped_id},
          web_irc::gen::lazy_format{string_tag<"/submit_form?channel={}&amp;connection_id={}">{}, escaped_id, escaped_connection_id});
      },
      [&](const web_irc::gen::status_channel&) {
        return std::format_to(ctx.out(), web_irc::gen::fmt::composer_frame.data(),
          "status-frame",
          web_irc::gen::lazy_format{string_tag<"/submit_status_form?connection_id={}">{}, escape_html(f.connection_id)});
      }
    }, f.ref);
  }
};

export template <typename Char>
struct std::formatter<submit_form, Char> {
  constexpr auto parse(auto& ctx) { return ctx.begin(); }
  constexpr auto format(submit_form f, auto& ctx) const {
    return std::visit(web_irc::gen::overloaded{
      [&](const web_irc::gen::regular_channel& ch) {
        return std::format_to(ctx.out(), web_irc::gen::fmt::submit_form.data(),
          web_irc::gen::lazy_format{string_tag<"/send_message?channel={}">{}, escape_html(ch.id)},
          escape_html(f.placeholder),
          escape_html(f.connection_id));
      },
      [&](const web_irc::gen::status_channel&) {
        return std::format_to(ctx.out(), web_irc::gen::fmt::submit_form.data(),
          "/send_status_message",
          escape_html(f.placeholder),
          escape_html(f.connection_id));
      }
    }, f.ref);
  }
};

export template <typename Char>
struct std::formatter<channel, Char> : std::formatter<std::string_view> {
  [[nodiscard]] constexpr auto format(channel ch, auto& ctx) const {
    const auto rules = channel_rules{.ref = ch.ref};
    const auto t = tab{.ref = ch.ref};
    const auto anchor = bottom_anchor{.ref = ch.ref};
    const auto frame = composer_frame{.ref = ch.ref, .connection_id = ch.connection_id};

    return ch.topic ? std::format_to(ctx.out(), "{}{}{}{}{}", rules, t, anchor, topic{.ref = ch.ref, .content = *ch.topic}, frame)
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

export template struct std::formatter<web_irc::gen::escape_html<std::string_view>, char>;
export template struct std::formatter<web_irc::gen::channel_ref, char>;

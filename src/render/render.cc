export module web_irc.render;
import std;
import web_irc.render.templates.fmt;
import web_irc.utils.lazy_format;
import web_irc.utils.overloaded;

using web_irc::utils::lazy_format;
using web_irc::utils::overloaded;
using web_irc::utils::string_tag;

namespace web_irc::render {

template <typename OutIt>
struct escape_html_iterator {
  OutIt out;

  escape_html_iterator& operator=(char c) {
    auto format = [&](std::string_view str) {
      return std::ranges::copy(str, std::move(out)).out;
    };
    out = c == '&'    ? format("&amp;")
          : c == '<'  ? format("&lt;")
          : c == '>'  ? format("&gt;")
          : c == '"'  ? format("&quot;")
          : c == '\'' ? format("&#39;")
                      : std::ranges::copy(std::views::single(c), std::move(out)).out;
    return *this;
  }

  escape_html_iterator& operator*() {
    return *this;
  }
  escape_html_iterator& operator++() {
    return *this;
  }
  escape_html_iterator operator++(int) {
    return *this;
  }

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

template <typename T>
escape_html(const T&) -> escape_html<T>;

export template <typename T>
struct formatter_wrapper {
  const T& value;
};

template <typename T, auto& fmt, typename Char>
struct formatter_helper {
  [[nodiscard]] constexpr auto format(auto& obj, auto& ctx) const {
    auto& [... vs] = obj;
    return std::format_to(ctx.out(), fmt.data(), escape_html(formatter_wrapper{vs})...);
  }
  [[nodiscard]] constexpr auto parse(auto& ctx) const {
    return ctx.begin();
  }
};

}  // namespace web_irc::render

template <typename T, typename Char>
struct std::formatter<web_irc::render::formatter_wrapper<T>, Char> : std::formatter<T> {
  [[nodiscard]] constexpr auto format(web_irc::render::formatter_wrapper<T> obj, auto& ctx) const {
    if constexpr(requires { std::formatter<T>::format(obj.value, ctx); }) {
      return std::formatter<T>::format(obj.value, ctx);
    } else {
      return std::format_to(ctx.out(), "{}", obj.value);
    }
  }

  [[nodiscard]] constexpr auto parse(auto& ctx) {
    return std::formatter<T>::parse(ctx);
  }
};

export template <typename T, typename Char>
struct std::formatter<web_irc::render::escape_html<T>, Char> : std::formatter<T, Char> {
  constexpr formatter() = default;
  [[nodiscard]] constexpr auto parse(auto& ctx) {
    return static_cast<std::formatter<T, Char>&>(*this).parse(ctx);
  }
  [[nodiscard]] constexpr auto format(const web_irc::render::escape_html<T>& e, auto& ctx) const {
    struct {
      web_irc::render::escape_html_iterator<decltype(ctx.out())> it;
      auto& out() {
        return this->it;
      }
    } new_ctx{.it = web_irc::render::escape_html_iterator{ctx.out()}};
    return static_cast<const std::formatter<T, Char>&>(*this).format(e.value, new_ctx).out;
  }
};

namespace web_irc::render {

export template <typename Id = std::string_view>
struct regular_channel {
  Id id;
};

export struct status_channel {};

export using channel_ref = std::variant<regular_channel<>, status_channel>;

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

}  // namespace web_irc::render

struct channel_formatter_base {
  web_irc::render::channel_spec spec = web_irc::render::channel_spec::none;
  constexpr auto parse(auto& ctx) {
    auto it = ctx.begin();
    auto end = ctx.end();
    if(it != end && *it != '}') {
      auto start = it;
      while(it != end && *it != '}')
        ++it;
      auto sv = std::string_view(start, it - start);
      if(sv == "tab_class")
        spec = web_irc::render::channel_spec::tab_class;
      else if(sv == "tab_href")
        spec = web_irc::render::channel_spec::tab_href;
      else if(sv == "tab_label")
        spec = web_irc::render::channel_spec::tab_label;
      else if(sv == "msg_class")
        spec = web_irc::render::channel_spec::msg_class;
      else if(sv == "users_class")
        spec = web_irc::render::channel_spec::users_class;
      else if(sv == "frame_class")
        spec = web_irc::render::channel_spec::frame_class;
      else if(sv == "frame_src")
        spec = web_irc::render::channel_spec::frame_src;
      else if(sv == "jump_class")
        spec = web_irc::render::channel_spec::jump_class;
      else if(sv == "topic_class")
        spec = web_irc::render::channel_spec::topic_class;
      else if(sv == "form_action")
        spec = web_irc::render::channel_spec::form_action;
      else if(sv == "anchor_id")
        spec = web_irc::render::channel_spec::anchor_id;
    }
    return it;
  };
};
template <typename Id, typename Char>
struct std::formatter<web_irc::render::regular_channel<Id>, Char> : channel_formatter_base {
  constexpr auto format(const web_irc::render::regular_channel<Id>& ch, auto& ctx) const {
    switch(spec) {
      case web_irc::render::channel_spec::tab_class:
        return std::format_to(ctx.out(), "tab-{}", ch.id);
      case web_irc::render::channel_spec::tab_href:
        return std::format_to(ctx.out(), "#tab-{}", ch.id);
      case web_irc::render::channel_spec::tab_label:
        return std::format_to(ctx.out(), "#{}", ch.id);
      case web_irc::render::channel_spec::msg_class:
        return std::format_to(ctx.out(), "inject-item-page-element-{}", ch.id);
      case web_irc::render::channel_spec::users_class:
        return std::format_to(ctx.out(), "inject-users-{}", ch.id);
      case web_irc::render::channel_spec::frame_class:
        return std::format_to(ctx.out(), "inject-frame-{}", ch.id);
      case web_irc::render::channel_spec::frame_src:
        return std::format_to(ctx.out(), "/submit_form?channel={}", ch.id);
      case web_irc::render::channel_spec::jump_class:
        return std::format_to(ctx.out(), "jump-{}", ch.id);
      case web_irc::render::channel_spec::topic_class:
        return std::format_to(ctx.out(), "page-{}", ch.id);
      case web_irc::render::channel_spec::form_action:
        return std::format_to(ctx.out(), "/send_message?channel={}", ch.id);
      case web_irc::render::channel_spec::anchor_id:
        return std::format_to(ctx.out(), "tab-{}", ch.id);
      default:
        return ctx.out();
    }
  }
};

export template <typename Char>
struct std::formatter<web_irc::render::status_channel, Char> : channel_formatter_base {
  constexpr auto format(const web_irc::render::status_channel&, auto& ctx) const {
    switch(spec) {
      case web_irc::render::channel_spec::tab_class:
        return std::format_to(ctx.out(), "status-tab");
      case web_irc::render::channel_spec::tab_href:
        return std::format_to(ctx.out(), "#status-tab");
      case web_irc::render::channel_spec::tab_label:
        return std::format_to(ctx.out(), "Status");
      case web_irc::render::channel_spec::msg_class:
        return std::format_to(ctx.out(), "status-inject-item-page-element");
      case web_irc::render::channel_spec::users_class:
        return std::format_to(ctx.out(), "status-users");
      case web_irc::render::channel_spec::frame_class:
        return std::format_to(ctx.out(), "status-frame");
      case web_irc::render::channel_spec::frame_src:
        return std::format_to(ctx.out(), "/submit_status_form");
      case web_irc::render::channel_spec::jump_class:
        return std::format_to(ctx.out(), "status-jump");
      case web_irc::render::channel_spec::topic_class:
        return std::format_to(ctx.out(), "status-topic");
      case web_irc::render::channel_spec::form_action:
        return std::format_to(ctx.out(), "/send_status_message");
      case web_irc::render::channel_spec::anchor_id:
        return std::format_to(ctx.out(), "status-tab");
      default:
        return ctx.out();
    }
  }
};

namespace web_irc::render {

export template <typename Channel>
struct bottom_anchor {
  Channel channel;
};

export template <typename Title = std::string_view, typename ConnectionId = std::string_view>
struct page_begin {
  Title title;
  ConnectionId connection_id;
};

export template <typename Channel>
struct channel_rules {
  Channel channel;
};

export template <typename Channel>
struct tab {
  Channel channel;
};

export template <typename Channel, typename Content>
struct topic {
  Channel channel;
  Content content;
};

export template <typename Channel,
                 typename Timestamp = std::string_view,
                 typename Author = std::string_view,
                 typename Content = std::string_view,
                 typename AuthorClass = std::string_view>
struct message {
  Channel channel;
  Timestamp timestamp;
  Author author;
  Content content;
  AuthorClass author_class;
};

export template <typename Channel, typename User = std::string_view>
struct user {
  Channel channel;
  User user;
};

export template <typename Channel, typename User = std::string_view>
struct user_hide {
  Channel channel;
  User user;
};

export template <typename Channel, typename User = std::string_view>
struct user_show {
  Channel channel;
  User user;
};

export template <typename Channel, typename Placeholder = std::string_view, typename ConnectionId = std::string_view>
struct submit_form {
  Channel channel;
  Placeholder placeholder;
  ConnectionId connection_id;
};

export template <typename Channel, typename ConnectionId = std::string_view>
struct composer_frame {
  Channel channel;
  ConnectionId connection_id;
};

export template <typename DefaultNick = std::string_view, typename DefaultChannels = std::string_view>
struct connect_form {
  DefaultNick default_nick;
  DefaultChannels default_channels;
};

export template <typename Channel,
                 typename Topic = std::string_view,
                 typename FormPlaceholder = std::string_view,
                 typename ConnectionId = std::string_view>
struct channel {
  Channel channel;
  std::optional<Topic> topic;
  FormPlaceholder form_placeholder;
  ConnectionId connection_id;
};

template <typename Channel, typename FormPlaceholder, typename ConnectionId>
channel(Channel, std::nullopt_t, FormPlaceholder, ConnectionId) -> channel<Channel, std::string_view, FormPlaceholder, ConnectionId>;

template <typename Channel, typename Topic, typename FormPlaceholder, typename ConnectionId>
channel(Channel, std::optional<Topic>, FormPlaceholder, ConnectionId) -> channel<Channel, Topic, FormPlaceholder, ConnectionId>;

}  // namespace web_irc::render
using namespace web_irc::render;  // NOLINT

template <typename Title, typename ConnectionId, typename Char>
struct std::formatter<page_begin<Title, ConnectionId>, Char>
    : formatter_helper<page_begin<Title, ConnectionId>, web_irc::render::fmt::page_begin, Char> {};

template <typename Channel, typename Char>
struct std::formatter<channel_rules<Channel>, Char> : formatter_helper<channel_rules<Channel>, web_irc::render::fmt::channel_rules, Char> {
};

template <typename Channel, typename Char>
struct std::formatter<tab<Channel>, Char> : formatter_helper<tab<Channel>, web_irc::render::fmt::tab, Char> {};

template <typename Channel, typename Content, typename Char>
struct std::formatter<topic<Channel, Content>, Char> : formatter_helper<topic<Channel, Content>, web_irc::render::fmt::topic, Char> {};

template <typename Channel, typename Timestamp, typename Author, typename Content, typename AuthorClass, typename Char>
struct std::formatter<message<Channel, Timestamp, Author, Content, AuthorClass>, Char>
    : formatter_helper<message<Channel, Timestamp, Author, Content, AuthorClass>, web_irc::render::fmt::message, Char> {};

template <typename Channel, typename User, typename Char>
struct std::formatter<user<Channel, User>, Char> : formatter_helper<user<Channel, User>, web_irc::render::fmt::user, Char> {};

template <typename Channel, typename User, typename Char>
struct std::formatter<user_hide<Channel, User>, Char> : formatter_helper<user_hide<Channel, User>, web_irc::render::fmt::user_hide, Char> {
};

template <typename Channel, typename User, typename Char>
struct std::formatter<user_show<Channel, User>, Char> : formatter_helper<user_show<Channel, User>, web_irc::render::fmt::user_show, Char> {
};

template <typename DefaultNick, typename DefaultChats, typename Char>
struct std::formatter<connect_form<DefaultNick, DefaultChats>, Char>
    : formatter_helper<connect_form<DefaultNick, DefaultChats>, web_irc::render::fmt::connect_form, Char> {};

template <typename Channel, typename Char>
struct std::formatter<bottom_anchor<Channel>, Char> : formatter_helper<bottom_anchor<Channel>, web_irc::render::fmt::bottom_anchor, Char> {
};

template <typename Channel, typename ConnectionId, typename Char>
struct std::formatter<composer_frame<Channel, ConnectionId>, Char> {
  constexpr auto parse(auto& ctx) {
    return ctx.begin();
  }
  constexpr auto format(auto&& f, auto& ctx) const {
    return overloaded{[&]<typename Cha>(const web_irc::render::regular_channel<Cha>& ch) {
                        return std::format_to(ctx.out(),
                                              web_irc::render::fmt::composer_frame.data(),
                                              lazy_format{string_tag<"inject-frame-{}">{}, escape_html{formatter_wrapper{ch.id}}},
                                              lazy_format{string_tag<"/submit_form?channel={}&amp;connection_id={}">{},
                                                          escape_html{formatter_wrapper{ch.id}},
                                                          escape_html{formatter_wrapper{f.connection_id}}});
                      },
                      [&](const web_irc::render::status_channel&) {
                        return std::format_to(ctx.out(),
                                              web_irc::render::fmt::composer_frame.data(),
                                              "status-frame",
                                              lazy_format{string_tag<"/submit_status_form?connection_id={}">{},
                                                          escape_html{formatter_wrapper{f.connection_id}}});
                      }}(f.channel);
  }
};

template <typename Channel, typename Placeholder, typename ConnectionId, typename Char>
struct std::formatter<submit_form<Channel, Placeholder, ConnectionId>, Char> {
  constexpr auto parse(auto& ctx) {
    return ctx.begin();
  }
  constexpr auto format(auto&& f, auto& ctx) const {
    return web_irc::utils::overloaded{[&]<typename Cha>(const web_irc::render::regular_channel<Cha>& ch) {
                                        return std::format_to(
                                            ctx.out(),
                                            web_irc::render::fmt::submit_form.data(),
                                            lazy_format{string_tag<"/send_message?channel={}">{}, escape_html{formatter_wrapper{ch.id}}},
                                            escape_html{formatter_wrapper{f.placeholder}},
                                            escape_html{formatter_wrapper{f.connection_id}});
                                      },
                                      [&](const web_irc::render::status_channel&) {
                                        return std::format_to(ctx.out(),
                                                              web_irc::render::fmt::submit_form.data(),
                                                              "/send_status_message",
                                                              escape_html{formatter_wrapper{f.placeholder}},
                                                              escape_html{formatter_wrapper{f.connection_id}});
                                      }}(f.channel);
  }
};

template <typename Char, typename Channel, typename Topic, typename FormPlaceholder, typename ConnectionId>
struct std::formatter<channel<Channel, Topic, FormPlaceholder, ConnectionId>, Char> : std::formatter<std::string_view> {
  [[nodiscard]] constexpr auto format(channel<Channel, Topic, FormPlaceholder, ConnectionId>& ch, auto& ctx) const {
    const auto rules = channel_rules{.channel = ch.channel};
    const auto t = tab{.channel = ch.channel};
    const auto anchor = bottom_anchor{.channel = ch.channel};
    const auto frame = composer_frame{.channel = ch.channel, .connection_id = ch.connection_id};

    return ch.topic ? std::format_to(ctx.out(), "{}{}{}{}{}", rules, t, anchor, topic{.channel = ch.channel, .content = *ch.topic}, frame)
                    : std::format_to(ctx.out(), "{}{}{}{}", rules, t, anchor, frame);
  }
};

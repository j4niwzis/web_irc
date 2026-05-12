export module web_irc.renderer;
import std;
import boost;
import web_irc.net.subscribe_manager;
export import web_irc.render;
import web_irc.core;
import web_irc.utils.lazy_format;
import web_irc.utils.channel_id;
import web_irc.utils.overloaded;
import web_irc.utils.range_string_formatter;
import web_irc.render.templates.html;

namespace web_irc {

namespace asio = boost::asio;

using asio::awaitable;
using utils::overloaded;
using utils::range_string_formatter;

template <typename T>
struct just {
  T obj;
};

template <typename T>
struct range {
  T obj;
};

template <typename... Ts>
struct variant {
  std::variant<Ts...> obj;
};

export class renderer {
 public:
  using channel = net::subscribe_manager::channel;

  template <typename... Ts>
  class chunk {
   public:
    auto heartbeat(this chunk self) {
      return std::move(self).event(std::string_view{"  "});
    }
    template <typename Title>
    auto page_begin(this chunk self, Title&& title) {
      return std::move(self)  //
          .event(render::page_begin{.title = std::forward<Title>(title), .connection_id = self.renderer_->connection_id_.subview()});
    }
    auto status_channel(this chunk self) {
      return std::move(self).event(render::channel{.channel = render::status_channel{},
                                                   .topic = std::nullopt,
                                                   .form_placeholder = "",
                                                   .connection_id = self.renderer_->connection_id_.subview()});
    }
    template <typename Author, typename Text>
    auto status_message(this chunk self, Author&& author, Text&& text) {
      return std::move(self).event(render::message{.channel = render::status_channel{},
                                                   .timestamp = self.renderer_->timestamp.subview(),
                                                   .author = std::forward<Author>(author),
                                                   .content = std::forward<Text>(text),
                                                   .author_class = std::string_view{}});
    }
    template <typename Text>
    auto status(this chunk self, Text&& text) {
      return std::move(self).status_message(std::string_view{"*"}, std::forward<Text>(text));
    }
    template <typename Channel, typename Author, typename Text>
    auto message(this chunk self, Channel&& ch, Author&& author, Text&& text, bool is_mention = false) {
      return std::move(self).event(render::message{.channel = render::regular_channel{std::forward<Channel>(ch)},
                                                   .timestamp = self.renderer_->timestamp.subview(),
                                                   .author = std::forward<Author>(author),
                                                   .content = std::forward<Text>(text),
                                                   .author_class = is_mention ? " mention" : std::string_view{}});
    }
    template <typename Channel, typename Text>
    auto system_message(this chunk self, Channel&& ch, Text&& text) {
      return std::move(self).message(std::forward<Channel>(ch), "*", std::forward<Text>(text));
    }
    auto channel(this chunk self, std::string_view channel, std::optional<std::string_view> topic = std::nullopt) {
      auto ch = utils::channel_id(channel);
      return std::move(self).event(render::channel{.channel = render::regular_channel{ch},
                                                   .topic = topic,
                                                   .form_placeholder = utils::lazy_format(utils::string_tag<"Message for #{}">{}, ch),
                                                   .connection_id = self.renderer_->connection_id_.subview()});
    }
    auto user(this chunk self, std::string_view ch, std::string_view user) {
      return std::move(self).event(render::user{.channel = render::regular_channel{ch}, .user = user});
    }
    auto hide_user(this chunk self, std::string_view ch, std::string_view user) {
      return std::move(self).event(render::user_hide{.channel = render::regular_channel{ch}, .user = user});
    }
    auto show_user(this chunk self, std::string_view ch, std::string_view user) {
      return std::move(self).event(render::user_show{.channel = render::regular_channel{ch}, .user = user});
    }
    template <typename T>
    auto event(this chunk self, T event) {
      return std::move(self).raw_add(just{std::move(event)});
    }
    auto empty(this chunk self) {
      return std::move(self).event(std::string_view{""});
    }
    auto beep(this chunk self) {
      return std::move(self).event(render::html::beep);
    }
    auto format_to(auto it) {
      std::apply(
          [&](auto&... events) {
            ((it = overloaded{[]<typename... VTypes>(this auto self, variant<VTypes...>& obj) {
                                return std::visit(
                                    [&](auto& obj) {
                                      return self(obj);
                                    },
                                    obj.obj);
                              },
                              [&]<typename T>(range<T>& obj) {
                                return std::format_to(it, "{}", range_string_formatter{obj.obj});
                              },
                              [&]<typename T>(just<T>& obj) {
                                return std::format_to(it, "{}", obj.obj);
                              }}(events)),
             ...);
          },
          events_);
      return it;
    }
    std::string to_string() {
      std::string response;
      format_to(std::back_inserter(response));
      return response;
    }
    void send() {
      format_to(std::back_inserter(renderer_->buffer));
    }
    template <typename F>
    auto variant_add(this chunk self, F&& f) {
      auto obj = std::invoke(std::forward<F>(f), chunk<>{*self.renderer_});
      auto tup = overloaded{
          []<typename... Ts2>(variant<Ts2...> obj)
            requires(std::is_same_v<chunk<>, Ts2> && ...)
          {
            return std::tuple{};
          },
          []<typename... Ts2>(this auto self, std::variant<Ts2...> obj) {
            using add = variant<std::decay_t<decltype(std::get<0>(std::get<Ts2>(obj).events_))>...>;
            using tail = variant<decltype(std::declval<Ts2>().tail())...>;
            return std::tuple_cat(std::make_tuple(std::visit(
                                      [](auto& obj) -> add {
                                        return {.obj = std::get<0>(std::move(obj).events_)};
                                      },
                                      obj)),
                                  self(std::visit(
                                      [](auto& obj) -> tail {
                                        if constexpr(requires { tail{.obj = std::move(obj).tail()}; }) {  // not same
                                          return tail{.obj = std::move(obj).tail()};
                                        } else {  // same
                                          return tail{.obj{std::in_place_index<0>, std::move(obj).tail()}};
                                        }
                                      },
                                      obj)));
          },
      }(std::move(obj));
      return renderer::chunk{*self.renderer_, std::tuple_cat(std::move(self.events_), std::move(tup))};
    }
    template <typename F>
    auto transform(this chunk self, F&& f) {
      return std::invoke(std::forward<F>(f), std::move(self));
    }
    template <typename Range>
    auto event_range(this chunk self, Range&& obj) {
      return std::move(self).raw_add(
          range{.obj = std::views::all(std::forward<Range>(obj)) | std::views::transform([renderer = self.renderer_](auto f) {
                         // TODO(j4niwzis): lazy
                         return f(chunk<>{*renderer}).to_string();
                       })});
    }

   private:
    template <typename T>
    constexpr chunk<Ts..., T> raw_add(this chunk self, T event) {
      return chunk<Ts..., T>{*self.renderer_, std::tuple_cat(std::move(self.events_), std::make_tuple(std::move(event)))};
    }
    explicit chunk(renderer& renderer, std::tuple<Ts...> events = {}) : renderer_(&renderer), events_(std::move(events)) {
    }
    [[nodiscard]] constexpr auto size() const {
      return sizeof...(Ts);
    }
    constexpr auto tail(this chunk self) {
      return renderer::chunk{*self.renderer_,
                             std::apply(
                                 [&]<typename T, typename... Ts2>(T&, Ts2&... objs) {
                                   return std::make_tuple(std::move(objs)...);
                                 },
                                 self.events_)};
    }
    friend renderer;
    renderer* renderer_;
    std::tuple<Ts...> events_;
  };

  awaitable<std::shared_ptr<channel>> subscribe() {
    return subscribe_manager_.subscribe();
  }
  void unsubscribe(std::shared_ptr<channel> ch) {
    subscribe_manager_.unsubscribe(std::move(ch));
  }
  explicit renderer(std::string_view connection_id = {}) : connection_id_(std::move(connection_id)) {
  }
  std::string_view& connection_id() {
    return connection_id_;
  }
  chunk<> render() {
    timestamp = utc_timestamp();
    return chunk<>{*this};
  }
  awaitable<void> commit() {
    co_await subscribe_manager_.send(std::move(buffer));
  }
  awaitable<void> close() {
    co_await subscribe_manager_.close();
  }

 private:
  net::subscribe_manager subscribe_manager_;
  std::string buffer;
  std::string_view connection_id_;
  std::string timestamp;
};

}  // namespace web_irc

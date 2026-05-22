export module web_irc.web_client;
import std;
import boost;
import web_irc.irc.client;
import web_irc.utils.channel_id;
import web_irc.utils.lazy_format;
import web_irc.utils.range_string_formatter;
import web_irc.renderer;
import web_irc.channel_manager;
import web_irc.config;

// NOLINTNEXTLINE
#define VARIANT_TER_OP(cond, t, f)                                                                                              \
  [&](auto true_branch, auto false_branch) {                                                                                    \
    return cond ? true_branch() : static_cast<std::variant<decltype(true_branch()), decltype(false_branch())>>(false_branch()); \
  }(                                                                                                                            \
      [&] {                                                                                                                     \
        return t;                                                                                                               \
      },                                                                                                                        \
      [&] {                                                                                                                     \
        return f;                                                                                                               \
      })

namespace web_irc {

namespace asio = boost::asio;
using asio::awaitable;
using utils::channel_id;

using namespace asio::experimental::awaitable_operators;

export class web_client : public std::enable_shared_from_this<web_client> {
 public:
  struct config {
    std::string server;
    std::uint16_t port;

    struct web_irc::irc::client::config irc_config;
  };
  using channel = renderer::channel;

  awaitable<void> connect(config cfg) {
    auto executor = co_await asio::this_coro::executor;
    std::println("[irc] resolving {}:{}", cfg.server, cfg.port);
    auto resolver = asio::ip::tcp::resolver(executor);
    auto endpoints = co_await resolver.async_resolve(cfg.server, std::to_string(cfg.port), asio::use_awaitable);

    if(endpoints.empty()) {
      throw std::runtime_error("could not resolve IRC server");
    }

    std::println("[irc] connecting to {}:{}", cfg.server, cfg.port);
    boost::asio::ip::tcp::socket socket(executor);
    co_await asio::async_connect(socket, endpoints, asio::use_awaitable);
    std::println("[irc] connected");
    client_.emplace(std::move(socket), cfg.irc_config);
    co_await client_->connect();
  }

  awaitable<void> run() {
    auto self = shared_from_this();
    is_running_ = true;
    std::exception_ptr saved_exception = nullptr;

    try {
      co_await render_startup();
      co_await event_loop();
    } catch(...) {
      saved_exception = std::current_exception();
    }

    is_running_ = false;
    co_await close();
    std::rethrow_exception(saved_exception);
  }

  explicit web_client(std::string connection_id) : connection_id_(std::move(connection_id)), renderer_(connection_id_) {
  }
  auto subscribe() {
    return renderer_.subscribe();
  }
  auto unsubscribe(std::shared_ptr<channel> ch) {
    return renderer_.unsubscribe(std::move(ch));
  }
  awaitable<void> close() {
    if(is_closed_) {
      co_return;
    }
    std::println("[irc] closing connection");
    is_closed_ = true;
    client_->close();
    co_await renderer_.close();
  }
  template <typename... Ts>
  awaitable<void> send(Ts... events) {
    co_await client_->send(std::move(events)...);
  }
  auto render() {
    return renderer_.render();
  }
  auto is_running() {
    return is_running_ && !is_closed_;
  }
  std::string_view nickname() {
    return client_->config().nickname;
  }
  awaitable<void> commit() {
    co_await renderer_.commit();
  }

 private:
  awaitable<void> render_startup() {
    auto content = std::to_array<std::string_view>({web_irc::config.version,
                                                    web_irc::config.copyright,
                                                    web_irc::config.url,
                                                    "Licensed under the GNU Affero General Public License, Version 3."});

    renderer_.render()
        .page_begin(web_irc::config.name)
        .status_channel()
        .event_range(content | std::views::transform([](auto str) {
                       return [=](auto chunk) {
                         return chunk.status_message(web_irc::config.short_name, str);
                       };
                     }))
        .status(utils::lazy_format(utils::string_tag<"Connecting to IRC as {}...">{}, nickname()))
        .send();

    co_await renderer_.commit();
  }
  awaitable<void> event_loop() {
    auto ex = co_await asio::this_coro::executor;
    asio::steady_timer timer(ex);

    for(;;) {
      co_await wait_with_heartbeat(timer);
      co_await process_pending_events();
      co_await renderer_.commit();
    }
  }
  // NOLINTNEXTLINE
  awaitable<void> wait_with_heartbeat(asio::steady_timer& timer) {
    auto ex = co_await asio::this_coro::executor;
    asio::experimental::channel<void(boost::system::error_code, std::exception_ptr)> ch{ex, 1};

    asio::co_spawn(ex, client_->read(), [&](std::exception_ptr ptr) {
      ch.async_send({}, ptr, asio::detached);
    });
    std::exception_ptr saved_exception = nullptr;

    for(;;) {
      timer.expires_after(std::chrono::seconds{30});
      auto response = co_await (ch.async_receive(asio::use_awaitable) || timer.async_wait(asio::use_awaitable));

      if(response.index() == 0) {
        if(auto ptr = std::get<0>(response)) {
          std::rethrow_exception(ptr);
        }
        break;
      }

      try {
        renderer_.render().heartbeat().send();
        co_await renderer_.commit();
      } catch(...) {
        saved_exception = std::current_exception();
      }

      if(saved_exception) {
        client_->socket().cancel();
        co_await ch.async_receive(asio::use_awaitable);
        std::rethrow_exception(saved_exception);
      }
    }
  }

  awaitable<void> process_pending_events() {
    while(auto event = client_->try_read_event()) {
      co_await handle(*std::move(event));
    }
  }
  awaitable<void> handle(mirc::event_t event) {
    return std::visit(
        [&](auto event) {
          return handle(std::move(event));
        },
        std::move(event));
  }

  auto user(std::string_view channel, std::string_view nick) {
    return [channel, nick, this](auto chunk) {
      auto data = channel_manager_.find(channel);
      return VARIANT_TER_OP(data->contains_user(nick),  //
                            chunk.show_user(channel, nick),
                            (data->users.emplace(nick), chunk.user(channel, nick)));
    };
  }
  auto channel_f(std::string_view channel, std::optional<std::string_view> topic = std::nullopt) {
    return [&, channel, topic](auto chunk) {
      auto data = channel_manager_.find(channel);
      return VARIANT_TER_OP(data->pushed, chunk.empty(), (data->pushed = true, chunk.channel(channel, topic)));
    };
  }
  awaitable<void> handle(mirc::event::join e) {
    auto ch = channel_id(e.channel);
    auto data = channel_manager_.find(ch);
    if(!data) {
      channel_manager_.add({.name = static_cast<std::string>(ch)});
      co_await client_->send(mirc::client::names{.channel = ch});
    }

    renderer_.render()
        .variant_add(channel_f(ch))
        .variant_add(user(ch, e.prefix.nick))
        .system_message(ch, utils::lazy_format(utils::string_tag<"{} has joined {}">{}, e.prefix.nick, e.channel))
        .send();
  }
  awaitable<void> handle(mirc::event::numeric::nam_reply e) {
    auto channel_name = channel_id(e.channel);
    auto prefixes = std::string_view("@+%&~");

    auto new_users = e.nick_views()  //
                     | std::views::transform([&](auto nick) {
                         return nick | std::views::drop_while([&](char c) {
                                  return prefixes.contains(c);
                                });
                       })  //
                     | std::views::filter([](auto nick) {
                         return !nick.empty();
                       })  //
                     | std::views::transform([&](auto nick) {
                         return std::string_view(std::ranges::begin(nick), std::ranges::end(nick));
                       })  //
                     | std::ranges::to<std::unordered_set<std::string_view>>();

    renderer_.render()
        .variant_add([&](auto chunk) {
          auto data = channel_manager_.find(channel_name);
          return VARIANT_TER_OP(data,
                                chunk.event_range(data->users  //
                                                  | std::views::filter([&](const auto& u) {
                                                      return !new_users.contains(u);
                                                    })  //
                                                  | std::views::transform([&](const auto& user) {
                                                      return [&](auto chunk) {
                                                        return chunk.hide_user(channel_name, user);
                                                      };
                                                    })),
                                chunk.empty());
        })
        .event_range(new_users | std::views::transform([&](const auto& nick) {
                       return [&](auto chunk) {
                         return chunk.variant_add(user(channel_name, nick));
                       };
                     }))
        .send();
    co_return;
  }
  awaitable<void> handle(mirc::event::priv_msg e) {
    std::println("[irc] priv_msg #{} <{}>: {}", e.target, e.prefix.nick, e.text);
    auto ch = channel_id(e.target);
    const bool is_mention = [&] {
      auto& cfg = client_->config();
      if(e.prefix.nick == cfg.nickname) {
        return false;
      }
      auto lower_nick = cfg.nickname | std::views::transform([](unsigned char c) {
                          return std::tolower(c);
                        });
      auto lower_text = e.text | std::views::transform([](unsigned char c) {
                          return std::tolower(c);
                        });
      if(std::ranges::contains_subrange(lower_text, lower_nick)) {
        return true;
      }
      return false;
    }();
    renderer_.render()
        .message(render::regular_channel{ch}, e.prefix.nick, e.text, is_mention)
        .transform([&](auto chunk) {
          return is_mention ? std::move(chunk).beep() : std::move(chunk).empty();
        })
        .send();
    co_return;
  }
  awaitable<void> handle(mirc::event::part e) {
    if(e.prefix.nick == client_->config().nickname) {
      channel_manager_.at(e.channel).joined = false;
    }
    renderer_.render()
        .hide_user(channel_id(e.channel), e.prefix.nick)
        .variant_add([&](auto chunk) {
          return VARIANT_TER_OP(
              e.reason.empty(),
              chunk.system_message(channel_id(e.channel),
                                   utils::lazy_format(utils::string_tag<"{} has left {}">{}, e.prefix.nick, e.channel)),
              chunk.system_message(channel_id(e.channel),
                                   utils::lazy_format(utils::string_tag<"{} has left {} ({})">{}, e.prefix.nick, e.channel, e.reason)));
        })
        .send();
    co_return;
  }
  awaitable<void> handle(mirc::event::quit e) {
    auto fn = [&](std::string_view ch) {
      return [&, ch](auto chunk) {
        return chunk  //
            .hide_user(ch, e.prefix.nick)
            .variant_add([&](auto chunk) {
              return VARIANT_TER_OP(
                  e.reason.empty(),
                  chunk.system_message(ch, utils::lazy_format(utils::string_tag<"{} has quit">{}, e.prefix.nick)),
                  chunk.system_message(ch, utils::lazy_format(utils::string_tag<"{} has quit ({})">{}, e.prefix.nick, e.reason)));
            });
      };
    };
    renderer_.render()
        .event_range(channel_manager_.channels()  //
                     | std::views::filter([&](const auto& data) {
                         return data.second.users.contains(e.prefix.nick);
                       })  //
                     | std::views::transform([&](const auto& data) {
                         return fn(data.first);
                       }))
        .send();
    co_return;
  }
  awaitable<void> handle(mirc::event::nick e) {
    if(e.prefix.nick == client_->config().nickname) {
      client_->config().nickname = std::string(e.new_nick);
    }
    auto rename_user = [&](std::string_view ch) {
      return [&, ch](auto chunk) {
        return chunk  //
            .hide_user(ch, e.prefix.nick)
            .variant_add(user(ch, e.new_nick));
      };
    };
    auto msg = utils::lazy_format(utils::string_tag<"{} is now known as {}">{}, e.prefix.nick, e.new_nick);
    renderer_.render()
        .event_range(channel_manager_.channels()  //
                     | std::views::filter([&](const auto& data) {
                         return data.second.users.contains(e.prefix.nick);
                       })  //
                     | std::views::transform([&](const auto& data) {
                         return rename_user(data.first);
                       }))
        .event_range(channel_manager_.channels()  //
                     | std::views::transform([&](const auto& data) {
                         return [&](auto chunk) {
                           return chunk  //
                               .system_message(data.first, msg);
                         };
                       }))
        .status(msg)
        .send();
    co_return;
  }
  awaitable<void> handle(mirc::event::mode e) {
    if(!e.target.empty() && e.target[0] == '#' && channel_manager_.find(e.target)) {
      // TODO(j4niwzis): lazy
      auto args_str = e.args | std::views::join_with(' ') | std::ranges::to<std::string>();
      renderer_.render()
          .variant_add([&](auto chunk) {
            return VARIANT_TER_OP(
                e.args.size() == 0,
                chunk.system_message(e.target, utils::lazy_format(utils::string_tag<"{} sets mode: {}">{}, e.prefix.nick, e.modes)),
                chunk.system_message(e.target,
                                     utils::lazy_format(utils::string_tag<"{} sets mode: {} {}">{}, e.prefix.nick, e.modes, args_str)));
          })
          .send();
    }
    co_return;
  }
  awaitable<void> handle(mirc::event::kick e) {
    if(e.user == client_->config().nickname) {
      channel_manager_.at(e.user).joined = false;
    }
    renderer_.render()
        .variant_add([&](auto chunk) {
          return VARIANT_TER_OP(
              e.reason.empty(),
              chunk.system_message(e.channel,
                                   utils::lazy_format(utils::string_tag<"{} kicked {} from {}">{}, e.prefix.nick, e.user, e.channel)),
              chunk.system_message(
                  e.channel,
                  utils::lazy_format(utils::string_tag<"{} kicked {} from {} ({})">{}, e.prefix.nick, e.user, e.channel, e.reason)));
        })
        .send();
    co_return;
  }

  awaitable<void> handle(mirc::event::ping e) {
    std::println("[irc] ping {}", e);
    co_await client_->send(mirc::client::pong{.server = e.server});
  }

  awaitable<void> handle(mirc::event::numeric_t e) {
    if(std::holds_alternative<mirc::event::numeric::nam_reply>(e)) {
      co_await handle(std::get<mirc::event::numeric::nam_reply>(e));
    } else if(e.code() == 366) {
    } else {
      renderer_.render().status(e).send();
    }
    co_return;
  }
  awaitable<void> handle(auto unknown_event) {
    std::println("Unknown event {}", unknown_event);
    co_return;
  }
  channel_manager channel_manager_;
  std::optional<irc::client> client_;
  std::string connection_id_;
  renderer renderer_;
  bool is_closed_{};
  bool is_running_{};
};

}  // namespace web_irc

export module web_irc.slash_commands;
import std;
import ctre;
import boost;
import mirc;
import web_irc.web_client;
import web_irc.utils.string_ops;
import web_irc.utils.overloaded;
import web_irc.utils.type_list;
import web_irc.utils.lazy_format;

namespace web_irc {
namespace asio = boost::asio;

using utils::ensure_hash_prefix;
using utils::overloaded;
using utils::trim_left;
using utils::type_list;

using asio::awaitable;

struct cmd_parse_data {
  const std::string& channel;
};

template <typename T>
class cmd_parse_helper {
 public:
  static constexpr std::optional<std::optional<T>> parse(std::string_view cmd, std::string_view args, cmd_parse_data data) {
    if(cmd != T::cmd) {
      return std::nullopt;
    }
    auto result = overloaded{[&]<typename T2>(type_list<T2>)  // lazy trick
                               requires requires { T2::parse_impl(args, data); }
                             {
                               return T2::parse_impl(args, data);
                             },
                             [&]<typename T2>(type_list<T2>) {
                               return T2::parse_impl(args);
                             }}(type_list<T>{});
    return overloaded{[](std::optional<T> obj) {
                        return obj;
                      },
                      [](T obj) {
                        return std::optional<T>{std::move(obj)};
                      }}(std::move(result));
  }
};

namespace cmd {

struct join : cmd_parse_helper<join> {
  static constexpr std::string_view cmd = "join";
  std::string channel;
  static constexpr std::optional<join> parse_impl(std::string_view args) {
    auto ch = ensure_hash_prefix(trim_left(args));
    return ch.empty() ? std::nullopt : std::optional{join{.channel = std::move(ch)}};
  }
};

struct part : cmd_parse_helper<part> {
  static constexpr std::string_view cmd = "part";
  std::string channel;
  std::string_view reason;
  static constexpr std::optional<part> parse_impl(std::string_view args, cmd_parse_data data) {
    auto ch_end = args.find(' ');
    auto ch_sv = ch_end == std::string_view::npos ? args : args.substr(0, ch_end);
    auto ch = ensure_hash_prefix(ch_sv.empty() ? data.channel : ch_sv);
    if(ch.empty()) {
      return std::nullopt;
    }
    return part{.channel = std::move(ch), .reason = ch_end == std::string_view::npos ? std::string_view{} : args.substr(ch_end + 1)};
  }
};

struct nick : cmd_parse_helper<nick> {
  static constexpr std::string_view cmd = "nick";
  std::string_view new_nick;
  static constexpr std::optional<nick> parse_impl(std::string_view args) {
    auto new_nick = trim_left(args);
    if(new_nick.empty()) {
      return std::nullopt;
    }
    return nick{.new_nick = new_nick};
  }
};

struct me : cmd_parse_helper<me> {
  static constexpr std::string_view cmd = "me";
  std::string_view args;
  std::string_view channel;
  static constexpr std::optional<me> parse_impl(std::string_view args, cmd_parse_data data) {
    if(data.channel.empty()) {
      return std::nullopt;
    }
    return me{.args = args, .channel = data.channel};
  }
};

struct msg : cmd_parse_helper<msg> {
  static constexpr std::string_view cmd = "msg";
  std::string_view target;
  std::string_view text;
  static constexpr std::optional<msg> parse_impl(std::string_view args) {
    auto target_end = args.find(' ');
    if(target_end == std::string_view::npos) {
      return std::nullopt;
    }
    auto target = args.substr(0, target_end);
    auto msg_text = args.substr(target_end + 1);
    if(target.empty()) {
      return std::nullopt;
    }
    return msg{.target = target, .text = msg_text};
  }
};

struct raw : cmd_parse_helper<raw> {
  static constexpr std::string_view cmd = "raw";
  std::string_view args;
  static constexpr raw parse_impl(std::string_view args) {
    return {.args = args};
  }
};

struct quit : cmd_parse_helper<quit> {
  static constexpr std::string_view cmd = "quit";
  std::string args;
  static constexpr quit parse_impl(std::string_view args) {
    return {.args = static_cast<std::string>(args)};
  }
};

struct unknown {
  std::string_view cmd;
};

}  // namespace cmd

export using cmd_t = std::variant<cmd::join, cmd::part, cmd::nick, cmd::me, cmd::msg, cmd::raw, cmd::quit, cmd::unknown>;

export std::optional<cmd_t> parse_slash_cmd(std::string_view text, const std::string& irc_channel) {
  if(text.empty() || text[0] != '/')
    return std::nullopt;
  constexpr auto pattern = ctll::fixed_string{R"(/(\w+)\s*(.*))"};
  if(auto m = ctre::match<pattern>(text)) {
    auto cmd = m.template get<1>().to_view();
    auto args = m.template get<2>().to_view();
    cmd_parse_data data{.channel = irc_channel};

    // safe
    return *overloaded{[&]<typename T, typename... Ts>(this auto self, type_list<T, Ts...>) {
                         return T::parse(cmd, args, data)
                             .transform([](auto value) {
                               return value.transform([&](auto value) {
                                 return cmd_t{std::move(value)};
                               });
                             })
                             .or_else([&] {
                               return self(type_list<Ts...>{});
                             });
                       },
                       [&](this auto self, type_list<cmd::unknown>) -> std::optional<std::optional<cmd_t>> {
                         return cmd::unknown{.cmd = cmd};
                       },
                       []<typename... Ts>(this auto self, type_list<std::variant<Ts...>>) {
                         return self(type_list<Ts...>{});
                       }}(type_list<cmd_t>{});
  }

  return std::nullopt;
}

export awaitable<void> execute_slash_cmd(web_client& irc, const cmd::join& cmd) {
  co_await irc.send(mirc::client::join{.channel = cmd.channel.subview()});
}
export awaitable<void> execute_slash_cmd(web_client& irc, const cmd::part& cmd) {
  co_await irc.send(mirc::client::part{.channel = cmd.channel.subview(), .reason = cmd.reason});
}
export awaitable<void> execute_slash_cmd(web_client& irc, const cmd::nick& cmd) {
  co_await irc.send(mirc::client::nick{.nickname = cmd.new_nick});
}
export awaitable<void> execute_slash_cmd(web_client& irc, const cmd::me& cmd) {
  co_await irc.send(mirc::client::action{.target = cmd.channel, .text = cmd.args});
  irc  //
      .render()
      .system_message(cmd.channel, utils::lazy_format(utils::string_tag<"{} {}">{}, irc.nickname(), cmd.args))
      .send();
  co_await irc.commit();
}
export awaitable<void> execute_slash_cmd(web_client& irc, const cmd::msg& cmd) {
  co_await irc.send(mirc::client::priv_msg{.target = cmd.target, .text = cmd.text});
  irc  //
      .render()
      .status_message(irc.nickname(), utils::lazy_format(utils::string_tag<"[-> {}] {}">{}, cmd.target, cmd.text))
      .send();
  co_await irc.commit();
}
export awaitable<void> execute_slash_cmd(web_client& irc, const cmd::raw& cmd) {
  co_await irc.send(cmd.args);
}
export awaitable<void> execute_slash_cmd(web_client& irc, const cmd::quit& cmd) {
  if(cmd.args.empty()) {
    irc  //
        .render()
        .status(std::string_view{"You have quit IRC"})
        .send();
  } else {
    irc  //
        .render()
        .status(utils::lazy_format(utils::string_tag<"You have quit IRC ({})">{}, cmd.args.subview()))
        .send();
  }
  co_await irc.commit();
  co_await irc.send(mirc::client::quit{.reason = cmd.args.subview()});
  co_await irc.close();
}
export awaitable<void> execute_slash_cmd(web_client& irc, const cmd::unknown& cmd) {
  irc  //
      .render()
      .status(utils::lazy_format(utils::string_tag<"Unknown command: /{}">{}, cmd.cmd))
      .send();
  co_await irc.commit();
}

export awaitable<void> execute_slash_cmd(web_client& irc, const cmd_t& cmd) {
  return std::visit(
      [&](const auto& cmd) {
        return execute_slash_cmd(irc, cmd);
      },
      cmd);
}
}  // namespace web_irc

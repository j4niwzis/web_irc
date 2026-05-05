export module web_irc.slash_commands;
import std;
import ctre;
import web_irc.irc;
import web_irc.utils;
import boost;
namespace web_irc {
namespace asio = boost::asio;
using asio::awaitable;
export struct parsed_cmd {
  std::string_view name;
  std::string_view args;
};
export std::optional<parsed_cmd> parse_slash_cmd(std::string_view text) {
  if(text.empty() || text[0] != '/') return std::nullopt;
  constexpr auto pattern = ctll::fixed_string{R"(/(\w+)\s*(.*))"};
  if(auto m = ctre::match<pattern>(text)) {
    return parsed_cmd{m.template get<1>().to_view(), m.template get<2>().to_view()};
  }
  return std::nullopt;
}
export awaitable<void> execute_slash_cmd(irc_client& irc, std::string_view cmd, std::string_view args, const std::string& irc_channel) {
  if(cmd == "join") {
    auto ch = ensure_hash_prefix(trim_left(args));
    if(!ch.empty()) co_await irc.join_channel(ch);
  } else if(cmd == "part") {
    auto rest = std::string_view(args);
    auto ch_end = rest.find(' ');
    auto ch_sv = ch_end == std::string_view::npos ? rest : rest.substr(0, ch_end);
    auto reason = ch_end == std::string_view::npos ? std::string_view{} : rest.substr(ch_end + 1);
    auto ch = ensure_hash_prefix(ch_sv.empty() ? irc_channel : ch_sv);
    if(!ch.empty()) co_await irc.part_channel(ch, std::string(reason));
  } else if(cmd == "nick") {
    auto new_nick = trim_left(args);
    if(!new_nick.empty()) co_await irc.change_nick(std::string(new_nick));
  } else if(cmd == "me") {
    if(irc_channel.empty()) co_return;
    co_await irc.send_action(irc_channel, args);
    irc.inject_message(irc_channel, "*", std::format("{} {}", irc.nickname(), args));
  } else if(cmd == "msg") {
    auto rest = std::string_view(args);
    auto target_end = rest.find(' ');
    if(target_end == std::string_view::npos) co_return;
    auto target = rest.substr(0, target_end);
    auto msg_text = rest.substr(target_end + 1);
    if(target.empty()) co_return;
    co_await irc.send_message(target, msg_text);
    irc.inject_message("status", irc.nickname(), std::format("[-> {}] {}", target, msg_text));
  } else if(cmd == "raw") {
    co_await irc.send_raw(std::string(args));
  } else if(cmd == "quit") {
    co_await irc.quit(std::string(args));
    irc.inject_message("status", "*", "You have quit IRC");
    irc.close();
  } else {
    irc.inject_message("status", "*", std::format("Unknown command: /{}", cmd));
  }
}
} // namespace web_irc

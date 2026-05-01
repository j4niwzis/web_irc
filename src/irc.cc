export module web_irc.irc;
import std;
import boost;
export import mirc;
import web_irc.core;
import web_irc.gen;

namespace web_irc {

namespace asio = boost::asio;
using asio::awaitable;

export struct irc_config {
  std::string server = "localhost";
  unsigned short port = 16667;
  std::string nickname = "webirc";
  std::string username = "webirc";
  std::string realname = "Web IRC Client";
  std::vector<std::string> channels = {"#test"};
};

namespace {
struct string_hash {
  using is_transparent = void;
  [[nodiscard]] std::size_t operator()(std::string_view sv) const noexcept {
    return std::hash<std::string_view>{}(sv);
  }
};

std::string escape_html(std::string_view sv) {
  std::string r;
  r.reserve(sv.size());
  for(char c : sv) {
    switch(c) {
      case '&':
        r += "&amp;";
        break;
      case '<':
        r += "&lt;";
        break;
      case '>':
        r += "&gt;";
        break;
      case '"':
        r += "&quot;";
        break;
      default:
        r += c;
        break;
    }
  }
  return r;
}

std::string_view channel_id(std::string_view ch) {
  if(!ch.empty() && ch[0] == '#')
    return ch.substr(1);
  return ch;
}

std::string format_params(const mirc::params& p) {
  return p | std::views::join_with(' ') | std::ranges::to<std::string>();
}

}  // namespace

export class irc_client {
 public:
  using chunk_channel = asio::experimental::channel<void(boost::system::error_code, std::string)>;

  explicit irc_client(asio::io_context& io, irc_config cfg) : socket_(io), cfg_(std::move(cfg)) {
    std::println("[irc] client created for {}", cfg_.server);
  }

  [[nodiscard]] const auto& config() const {
    return cfg_;
  }
  [[nodiscard]] std::string_view nickname() const {
    return cfg_.nickname;
  }

  void set_connection_id(std::string id) {
    connection_id_ = std::move(id);
  }

  [[nodiscard]] bool is_connected() const {
    return !closed_;
  }

  void close() {
    if(closed_)
      return;
    closed_ = true;
    std::println("[irc] closing connection");
    boost::beast::error_code ec;
    socket_.close(ec);
    push_chunk("");
  }

  [[nodiscard]] std::shared_ptr<chunk_channel> subscribe() {
    auto ch = std::make_shared<chunk_channel>(socket_.get_executor(), 64);
    subs_.push_back(ch);
    return ch;
  }

  void unsubscribe(std::shared_ptr<chunk_channel> ch) {
    if(!ch)
      return;
    ch->close();
    std::erase(subs_, ch);
  }

  awaitable<std::string> next_chunk(std::shared_ptr<chunk_channel> ch, std::chrono::seconds timeout = std::chrono::seconds(30)) {
    auto ex = co_await asio::this_coro::executor;
    asio::steady_timer timer(ex);
    timer.expires_after(timeout);

    auto [order, ec_msg, msg, ec_timer] =
        co_await asio::experimental::make_parallel_group(ch->async_receive(asio::deferred), timer.async_wait(asio::deferred))
            .async_wait(asio::experimental::wait_for_one(), asio::use_awaitable);

    if(!ec_msg) {
      std::println("[irc] next_chunk ({} bytes)", msg.size());
      co_return msg;
    }
    co_return "";
  }

  awaitable<void> connect() {
    std::println("[irc] resolving {}:{}", cfg_.server, cfg_.port);
    auto resolver = asio::ip::tcp::resolver(co_await asio::this_coro::executor);
    auto endpoints = co_await resolver.async_resolve(cfg_.server, std::to_string(cfg_.port), asio::use_awaitable);

    if(endpoints.empty()) {
      throw std::runtime_error("could not resolve IRC server");
    }

    std::println("[irc] connecting to {}:{}", cfg_.server, cfg_.port);
    co_await asio::async_connect(socket_, endpoints, asio::use_awaitable);
    std::println("[irc] connected");

    co_await send(std::format("{}", mirc::client::nick{.nickname = cfg_.nickname}));
    co_await send(std::format("{}", mirc::client::user{.username = cfg_.username, .realname = cfg_.realname}));

    for(const auto& ch : cfg_.channels) {
      co_await send(std::format("{}", mirc::client::join{.channel = ch}));
    }

    co_await read_loop();
  }

  awaitable<void> send_message(std::string_view target, std::string_view text) {
    std::println("[irc] send_message {}: {}", target, text);
    co_await send(std::format("{}", mirc::client::priv_msg{.target = target, .text = text}));
  }

  awaitable<void> send_action(std::string_view target, std::string_view text) {
    auto action_text = std::format("\001ACTION {}\001", text);
    mirc::client::priv_msg cmd{.target = target, .text = action_text};
    std::println("[irc] send_action {}: {}", target, text);
    co_await send(std::format("{}", cmd));
  }

  awaitable<void> join_channel(std::string_view channel) {
    mirc::client::join cmd{.channel = channel};
    std::println("[irc] join_channel {}", channel);
    co_await send(std::format("{}", cmd));
  }

  awaitable<void> part_channel(std::string_view channel, std::string_view reason) {
    mirc::client::part cmd{.channel = channel, .reason = reason};
    std::println("[irc] part_channel {} ({})", channel, reason);
    co_await send(std::format("{}", cmd));
  }

  awaitable<void> change_nick(std::string_view nick) {
    mirc::client::nick cmd{.nickname = nick};
    std::println("[irc] change_nick {}", nick);
    co_await send(std::format("{}", cmd));
  }

  awaitable<void> send_raw(std::string line) {
    std::println("[irc] send_raw {}", line);
    co_await send(std::move(line));
  }

  awaitable<void> quit(std::string_view reason) {
    mirc::client::quit cmd{.reason = reason};
    std::println("[irc] quit ({})", reason);
    co_await send(std::format("{}", cmd));
  }

  void inject_message(std::string_view target, std::string_view author, std::string_view text) {
    std::println("[irc] inject_message {} <{}>: {}", target, author, text);
    auto ch = channel_id(target);
    auto author_escaped = escape_html(author);
    auto text_escaped = escape_html(text);
    push_chunk(std::format(
        "{}", web_irc::gen::message{.channel = ch, .timestamp = utc_timestamp(), .author = author_escaped, .content = text_escaped}));
  }

 private:
  asio::ip::tcp::socket socket_;
  irc_config cfg_;
  std::string connection_id_;
  asio::streambuf read_buf_;
  std::vector<std::shared_ptr<chunk_channel>> subs_;
  std::unordered_set<std::string, string_hash, std::equal_to<>> joined_channels_;
  std::unordered_map<std::string, std::unordered_set<std::string, string_hash, std::equal_to<>>, string_hash, std::equal_to<>>
      channel_users_;
  std::unordered_set<std::string, string_hash, std::equal_to<>> pushed_channels_;
  bool closed_ = false;

  awaitable<void> send(std::string line) {
    line += "\r\n";
    std::println("[irc] -> {}", std::string_view(line).substr(0, line.size() - 2));
    co_await asio::async_write(socket_, asio::buffer(line), asio::use_awaitable);
  }

  void push_chunk(std::string chunk) {
    std::println("[irc] push_chunk ({} bytes) to {} subscriber(s)", chunk.size(), subs_.size());
    for(auto it = subs_.begin(); it != subs_.end();) {
      auto& ch = *it;
      if(!ch->try_send(boost::system::error_code{}, chunk) && !ch->is_open()) {
        it = subs_.erase(it);
      } else {
        ++it;
      }
    }
  }

  void push_system_message(std::string_view channel, std::string_view text) {
    auto ch = channel_id(channel);
    push_chunk(
        std::format("{}", web_irc::gen::message{.channel = ch, .timestamp = utc_timestamp(), .author = "*", .content = escape_html(text)}));
  }

  void push_status(std::string_view text) {
    push_system_message("status", text);
  }

  void push_channel(std::string_view channel, std::optional<std::string_view> topic = std::nullopt) {
    if(connection_id_.empty())
      return;
    pushed_channels_.insert(std::string(channel));
    auto ch = channel_id(channel);
    push_chunk(std::format(
        "{}",
        web_irc::gen::channel{
            .id = ch, .topic = topic, .form_placeholder = std::format("Message for #{}", ch), .connection_id = connection_id_}));
  }

  struct read_loop_dispatcher {
    irc_client* self;
    std::string_view line;
    const mirc::event_t& ev;

    awaitable<void> operator()(const mirc::event::ping& ping) const {
      std::println("[irc] ping from {}", ping.server);
      co_await self->send(std::format("{}", mirc::client::pong{.server = ping.server}));
    }

    template <typename T>
    awaitable<void> operator()(const T&) const {
      co_await self->handle_event(ev);
    }
  };

  awaitable<void> read_loop() {
    std::println("[irc] read_loop started");
    try {
      while(socket_.is_open()) {
        auto bytes = co_await asio::async_read_until(socket_, read_buf_, "\n", asio::use_awaitable);

        auto it = asio::buffers_begin(read_buf_.data());
        auto line_view = std::string_view(&*it, bytes);
        if(!line_view.empty() && line_view.back() == '\n')
          line_view.remove_suffix(1);
        if(!line_view.empty() && line_view.back() == '\r')
          line_view.remove_suffix(1);

        std::string line(line_view);
        read_buf_.consume(bytes);

        std::println("[irc] <- {}", line);

        auto ev = mirc::parse_event(line);
        if(!ev) {
          std::println("[irc] parse_event failed for: {}", line);
          continue;
        }

        co_await std::visit(read_loop_dispatcher{this, line, *ev}, *ev);
      }
    } catch(...) {
      close();
      throw;
    }
    close();
    std::println("[irc] read_loop ended");
  }

  awaitable<void> handle_event(const mirc::event_t& ev) {
    co_await std::visit(
        [this](const auto& e) {
          return handle(e);
        },
        ev);
  }

  awaitable<void> handle(const mirc::event::priv_msg& e) {
    std::println("[irc] priv_msg #{} <{}>: {}", e.target, e.prefix.nick, e.text);
    auto ch = channel_id(e.target);
    auto nick_escaped = escape_html(e.prefix.nick);
    auto text_sv = std::string_view(e.text);
    bool is_action = text_sv.size() > 8 && text_sv.starts_with("\001ACTION ") && text_sv.ends_with("\001");
    auto content = is_action ? std::format("{} {}", nick_escaped, escape_html(text_sv.substr(8, text_sv.size() - 9))) : escape_html(e.text);
    push_chunk(std::format(
        "{}",
        web_irc::gen::message{.channel = ch, .timestamp = utc_timestamp(), .author = is_action ? "*" : nick_escaped, .content = content}));
    co_return;
  }

  awaitable<void> handle(const mirc::event::join& e) {
    if(e.prefix.nick == cfg_.nickname) {
      joined_channels_.insert(std::string(e.channel));
      if(!pushed_channels_.contains(e.channel)) {
        push_channel(e.channel);
      }
      co_await request_names(e.channel);
    }
    push_user(e.channel, e.prefix.nick);
    push_system_message(e.channel, std::format("{} has joined {}", e.prefix.nick, e.channel));
    co_return;
  }

  awaitable<void> handle(const mirc::event::part& e) {
    if(e.prefix.nick == cfg_.nickname) {
      if(auto it = joined_channels_.find(e.channel); it != joined_channels_.end()) {
        joined_channels_.erase(it);
      }
    } else {
      hide_user(e.channel, e.prefix.nick);
    }
    auto msg = e.reason.empty() ? std::format("{} has left {}", e.prefix.nick, e.channel)
                                : std::format("{} has left {} ({})", e.prefix.nick, e.channel, e.reason);
    push_system_message(e.channel, msg);
    co_return;
  }

  awaitable<void> handle(const mirc::event::quit& e) {
    auto msg = e.reason.empty() ? std::format("{} has quit", e.prefix.nick) : std::format("{} has quit ({})", e.prefix.nick, e.reason);
    for(const auto& [ch, users] : channel_users_) {
      if(users.contains(e.prefix.nick)) {
        hide_user(ch, e.prefix.nick);
        push_system_message(ch, msg);
      }
    }
    co_return;
  }

  awaitable<void> handle(const mirc::event::nick& e) {
    auto msg = std::format("{} is now known as {}", e.prefix.nick, e.new_nick);
    if(e.prefix.nick == cfg_.nickname) {
      cfg_.nickname = std::string(e.new_nick);
    }
    for(const auto& [ch, users] : channel_users_) {
      if(users.contains(e.prefix.nick)) {
        hide_user(ch, e.prefix.nick);
        push_user(ch, e.new_nick);
      }
      push_system_message(ch, msg);
    }
    for(const auto& ch : joined_channels_) {
      auto it = channel_users_.find(ch);
      if(it == channel_users_.end() || !it->second.contains(e.prefix.nick)) {
        push_system_message(ch, msg);
      }
    }
    push_status(msg);
    co_return;
  }

  awaitable<void> handle(const mirc::event::mode& e) {
    if(!e.target.empty() && e.target[0] == '#' && joined_channels_.contains(e.target)) {
      auto args_str = format_params(e.args);
      auto mode_text = args_str.empty() ? std::format("{} sets mode: {}", e.prefix.nick, e.modes)
                                        : std::format("{} sets mode: {} {}", e.prefix.nick, e.modes, args_str);
      push_system_message(e.target, mode_text);
    }
    co_return;
  }

  awaitable<void> handle(const mirc::event::kick& e) {
    if(e.user == cfg_.nickname) {
      if(auto it = joined_channels_.find(e.channel); it != joined_channels_.end()) {
        joined_channels_.erase(it);
      }
    } else {
      hide_user(e.channel, e.user);
    }
    auto msg = e.reason.empty() ? std::format("{} kicked {} from {}", e.prefix.nick, e.user, e.channel)
                                : std::format("{} kicked {} from {} ({})", e.prefix.nick, e.user, e.channel, e.reason);
    push_system_message(e.channel, msg);
    co_return;
  }

  awaitable<void> handle(const mirc::event::numeric_t& e) {
    if(std::holds_alternative<mirc::event::numeric::nam_reply>(static_cast<const mirc::event::numeric::variant&>(e))) {
      co_await handle_names_reply(std::get<mirc::event::numeric::nam_reply>(static_cast<const mirc::event::numeric::variant&>(e)));
    } else if(e.code() == 366) {
    } else {
      push_status(std::format("{}", e));
    }
    co_return;
  }

  template <typename T>
  awaitable<void> handle(const T&) {
    std::println("[irc] unhandled event type");
    co_return;
  }

  awaitable<void> request_names(std::string_view channel) {
    co_await send(std::format("NAMES {}", channel));
  }

  awaitable<void> handle_names_reply(const mirc::event::numeric::nam_reply& e) {
    auto channel_name = e.channel;
    auto prefixes = std::string_view("@+%&~");

    std::unordered_set<std::string, string_hash, std::equal_to<>> new_users;

    for(auto nick : e.nick_views()) {
      auto clean = nick | std::views::drop_while([&](char c) {
                     return prefixes.contains(c);
                   });
      if(!clean.empty()) {
        new_users.emplace(std::string_view(std::ranges::begin(clean), std::ranges::end(clean)));
      }
    }

    if(auto it = channel_users_.find(channel_name); it != channel_users_.end()) {
      for(const auto& user : it->second | std::views::filter([&](const auto& u) {
                               return !new_users.contains(u);
                             })) {
        hide_user(channel_name, user);
      }
    }

    for(const auto& user : new_users) {
      push_user(channel_name, user);
    }

    co_return;
  }

  void push_user(std::string_view channel, std::string_view nick) {
    auto ch_id = channel_id(channel);
    std::string nick_str(nick);
    auto& users = channel_users_[std::string(channel)];
    if(!users.contains(nick_str)) {
      users.insert(nick_str);
      push_chunk(std::format("{}", web_irc::gen::user{.channel = ch_id, .user = nick_str}));
    } else {
      push_chunk(std::format("{}", web_irc::gen::user_show{.channel = ch_id, .user = nick_str}));
    }
  }

  void hide_user(std::string_view channel, std::string_view nick) {
    auto ch_id = channel_id(channel);
    std::string nick_str(nick);
    push_chunk(std::format("{}", web_irc::gen::user_hide{.channel = ch_id, .user = nick_str}));
  }

  void show_user(std::string_view channel, std::string_view nick) {
    auto ch_id = channel_id(channel);
    std::string nick_str(nick);
    std::string ch_str(channel);
    channel_users_[ch_str].insert(nick_str);
    push_chunk(std::format("{}", web_irc::gen::user_show{.channel = ch_id, .user = nick_str}));
  }
};

}  // namespace web_irc

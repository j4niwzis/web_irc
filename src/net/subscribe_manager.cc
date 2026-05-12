export module web_irc.net.subscribe_manager;
import std;
import boost;

namespace web_irc::net {

namespace asio = boost::asio;

using asio::awaitable;

export class subscribe_manager {
 public:
  using channel = asio::experimental::channel<void(boost::system::error_code, std::string)>;
  awaitable<std::shared_ptr<channel>> subscribe() {
    auto ch = std::make_shared<channel>(co_await asio::this_coro::executor);
    subs_.push_back(ch);
    co_return ch;
  }
  void unsubscribe(std::shared_ptr<channel> ch) {
    if(!ch) {
      return;
    }
    ch->close();
    std::erase(subs_, ch);
  }
  awaitable<void> send(std::string chunk) {
    if(chunk.empty()) {
      co_return;
    }
    if(subs_.size() == 0) {
      throw std::runtime_error{"Dead channel"};
    }
    std::println("[irc] push_chunk ({} bytes) to {} subscriber(s)", chunk.size(), subs_.size());
    // NOLINTNEXTLINE
    co_await send([&](auto& ch) -> awaitable<void> {
      co_await ch->async_send(boost::system::error_code{}, chunk, asio::use_awaitable);
    });
  }
  awaitable<void> close() {
    // NOLINTNEXTLINE
    co_await send([](auto& ch) -> awaitable<void> {
      co_await ch->async_send(asio::error::eof, "", asio::use_awaitable);
    });
  }

 private:
  awaitable<void> send(auto f) {
    for(auto it = subs_.begin(); it != subs_.end();) {
      auto& ch = *it;

      try {
        co_await f(ch);
        ++it;
      } catch(const std::exception& e) {
        std::println("[irc] Failed to send to subscriber: {}", e.what());
        ch->close();
        it = subs_.erase(it);
      }
    }
  }
  std::vector<std::shared_ptr<channel>> subs_;
};

}  // namespace web_irc::net

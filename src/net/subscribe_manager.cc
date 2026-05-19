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
    auto ex = co_await asio::this_coro::executor;
    auto lock = co_await mutex.async_scoped_lock(asio::use_awaitable);
    auto ch = std::make_shared<channel>(ex);
    subs_.push_back(ch);
    co_return ch;
  }
  awaitable<void> unsubscribe(std::shared_ptr<channel> ch) {
    if(!ch) {
      co_return;
    }
    auto lock = co_await mutex.async_scoped_lock(asio::use_awaitable);

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
    auto lock = co_await mutex.async_scoped_lock(asio::use_awaitable);
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
  avast::asio::async_mutex mutex;
  std::vector<std::shared_ptr<channel>> subs_;
};

}  // namespace web_irc::net

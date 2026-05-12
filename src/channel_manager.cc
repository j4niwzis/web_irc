export module web_irc.channel_manager;
import std;

namespace web_irc {

struct string_hash {
  using is_transparent = void;
  [[nodiscard]] constexpr std::size_t operator()(std::string_view sv) const noexcept {
    return std::hash<std::string_view>{}(sv);
  }
};

export class channel_data {
 public:
  std::string name;  // copy
  std::optional<std::string> topic;
  std::unordered_set<std::string, string_hash, std::equal_to<>> users;
  bool pushed = false;
  bool joined = true;
  [[nodiscard]] constexpr bool contains_user(std::string_view str) const {
    return users.find(str) != users.end();
  }
};

export class channel_manager {
 public:
  constexpr channel_data& add(channel_data data) {
    return channels_.emplace(data.name, std::move(data)).first->second;
  }

  constexpr channel_data& at(std::string_view channel) {
    auto ptr = find(channel);
    if(!ptr) {
      throw std::runtime_error{"Not found channel"};
    }
    return *ptr;
  }
  constexpr channel_data* find(std::string_view channel) {
    auto it = channels_.find(channel);
    return it != channels_.end() ? &it->second : nullptr;
  }
  auto& channels() {
    return channels_;
  }

 private:
  // TODO(j4niwzis): use Boost.MultiIndex
  std::unordered_map<std::string, channel_data, string_hash, std::equal_to<>> channels_;
};

}  // namespace web_irc

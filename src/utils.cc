export module web_irc.utils;
import std;
import web_irc.range_string_formatter;
namespace web_irc {
export std::string generate_uuid() {
  static std::mutex mtx;
  static std::mt19937_64 gen([] {
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    if(!urandom) {
      throw std::runtime_error("Cannot open /dev/urandom");
    }
    alignas(std::uint64_t) char arr[sizeof(std::uint64_t)];
    urandom.read(arr, sizeof(std::uint64_t));
    if(!urandom) {
      throw std::runtime_error("Failed to read from /dev/urandom");
    }
    return std::bit_cast<std::uint64_t>(arr);
  }());
  std::lock_guard<std::mutex> lock(mtx);
  constexpr std::array<char, 16> hex_chars = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
  auto random_hex = [&]() { return hex_chars[std::uniform_int_distribution<>(0, 15)(gen)]; };
  auto random_variant = [&]() { return hex_chars[std::uniform_int_distribution<>(8, 11)(gen)]; };
  auto generate_segment = [&](int count) {
    return std::views::iota(0, count) | std::views::transform([&](int) { return random_hex(); });
  };
  return std::format("{}-{}-4{}-{}{}-{}",
                     range_string_formatter{generate_segment(8)},
                     range_string_formatter{generate_segment(4)},
                     range_string_formatter{generate_segment(3)},
                     random_variant(),
                     range_string_formatter{generate_segment(3)},
                     range_string_formatter{generate_segment(12)});
}
export std::string url_decode(std::string_view sv) {
  std::string r;
  r.reserve(sv.size());
  for(std::size_t i = 0; i < sv.size(); ++i) {
    if(sv[i] == '+') {
      r += ' ';
      continue;
    } else if(sv[i] == '%' && i + 2 < sv.size()) {
      auto hex_val = [](char c) -> int {
        if(c >= '0' && c <= '9') return c - '0';
        if(c >= 'a' && c <= 'f') return c - 'a' + 10;
        if(c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
      };
      int high = hex_val(sv[i + 1]);
      int low = hex_val(sv[i + 2]);
      if(high >= 0 && low >= 0) {
        r += static_cast<char>((high << 4) | low);
        i += 2;
        continue;
      }
    }
    r += sv[i];
  }
  return r;
}
export std::string extract_value(std::string_view data, std::string_view key) {
  for(auto part : data | std::views::split('&')) {
    auto eq = std::ranges::find(part, '=');
    if(eq == part.end()) continue;
    if(std::string_view(part.begin(), eq) == key) {
      return url_decode(std::string_view(std::next(eq), part.end()));
    }
  }
  return {};
}
export std::string_view trim_left(std::string_view sv) {
  auto pos = sv.find_first_not_of(' ');
  return pos == std::string_view::npos ? "" : sv.substr(pos);
}
export std::string ensure_hash_prefix(std::string_view sv) {
  return sv.empty() || sv[0] == '#' ? std::string(sv) : std::format("#{}", sv);
}
export std::vector<std::string> parse_channels(std::string_view input) {
  std::vector<std::string> result;
  std::size_t start = 0;
  while(start <= input.size()) {
    auto end = input.find(' ', start);
    auto part = input.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
    auto sv = trim_left(part);
    auto ch = ensure_hash_prefix(sv);
    if(!ch.empty()) result.push_back(std::move(ch));
    if(end == std::string_view::npos) break;
    start = end + 1;
  }
  return result;
}
export std::string form_placeholder(std::string_view channel) {
  if(channel.empty()) return "Message";
  return std::format("Message for #{}", channel);
}
} // namespace web_irc

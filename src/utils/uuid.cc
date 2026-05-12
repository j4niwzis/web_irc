export module web_irc.utils.uuid;
import web_irc.utils.range_string_formatter;
import std;

namespace web_irc::utils {

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
  auto random_hex = [&]() {
    return hex_chars[std::uniform_int_distribution<>(0, 15)(gen)];
  };
  auto random_variant = [&]() {
    return hex_chars[std::uniform_int_distribution<>(8, 11)(gen)];
  };
  auto generate_segment = [&](int count) {
    return std::views::iota(0, count) | std::views::transform([&](int) {
             return random_hex();
           });
  };
  return std::format("{}-{}-4{}-{}{}-{}",
                     range_string_formatter{generate_segment(8)},
                     range_string_formatter{generate_segment(4)},
                     range_string_formatter{generate_segment(3)},
                     random_variant(),
                     range_string_formatter{generate_segment(3)},
                     range_string_formatter{generate_segment(12)});
}

}  // namespace web_irc::utils

export module web_irc.utils.uuid;
import web_irc.utils.range_string_formatter;
import web_irc.utils.lazy_format;
import std;

namespace web_irc::utils {

static std::ifstream urandom("/dev/urandom", std::ios::binary);
static std::array<char, 4096> buffer;
static std::size_t buffer_pos = 4096;

static constexpr std::array<char, 16> hex_chars = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

static unsigned char next_byte() {
  if(buffer_pos >= buffer.size()) {
    do {
      urandom.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
      if(!urandom && !urandom.eof()) {
        throw std::runtime_error("Read from /dev/urandom failed");
      }
    } while(urandom.gcount() == 0);
    buffer_pos = 0;
  }
  return static_cast<unsigned char>(buffer[buffer_pos++]);
}

auto generate_segment(int hex_count) {
  return std::views::iota(0, hex_count / 2)  //
         | std::views::transform([](auto) {
             auto byte = next_byte();
             return std::array{byte >> 4, byte & 0x0F};
           })                //
         | std::views::join  //
         | std::views::transform([](auto num) {
             return hex_chars[num];
           });
}

char random_variant() {
  unsigned char v = (next_byte() & 0x3f) | 0x80;
  return hex_chars[v >> 4];
}

export std::string generate_uuid() {
  if(!urandom.is_open()) {
    urandom.open("/dev/urandom", std::ios::binary);
  }
  if(!urandom) {
    throw std::runtime_error("Cannot open /dev/urandom");
  }
  auto byte = next_byte();
  return std::format("{}-{}-4{}{}-{}{}{}-{}",
                     range_string_formatter{generate_segment(8)},
                     range_string_formatter{generate_segment(4)},
                     range_string_formatter{generate_segment(2)},
                     hex_chars[byte >> 4],
                     random_variant(),
                     range_string_formatter{generate_segment(2)},
                     hex_chars[byte & 0x0F],
                     range_string_formatter{generate_segment(12)});
}

}  // namespace web_irc::utils

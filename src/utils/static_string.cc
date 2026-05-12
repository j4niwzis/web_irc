export module web_irc.utils.static_string;
import std;

namespace web_irc::utils {

export template <std::size_t N>
struct static_string {
  char data[N];
  constexpr static_string(const char (&str)[N]) {
    for(std::size_t i = 0; i < N; ++i)
      data[i] = str[i];
  }
  constexpr std::size_t size() const {
    return N - 1;
  }
  constexpr const char* c_str() const {
    return data;
  }
};

export template <std::size_t N>
static_string(const char (&str)[N]) -> static_string<N>;

export template <static_string>
struct string_tag {};

}  // namespace web_irc::utils

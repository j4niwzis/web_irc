export module web_irc.utils.lazy_format;
export import web_irc.utils.static_string;
import std;

namespace web_irc::utils {

export template <static_string fmt, typename... Ts>
struct lazy_format {
  explicit constexpr lazy_format(string_tag<fmt>, const Ts&... args) : values{args...} {
  }
  std::tuple<Ts...> values;
};

}  // namespace web_irc::utils

template <web_irc::utils::static_string fmt, typename... Ts, typename Char>
struct std::formatter<web_irc::utils::lazy_format<fmt, Ts...>, Char> {
  constexpr auto parse(auto& ctx) const {
    return ctx.begin();
  }
  constexpr auto format(auto& lf, auto& ctx) const {
    return std::apply(
        [&](auto&&... args) {
          return std::format_to(ctx.out(), fmt.data, args...);
        },
        lf.values);
  }
};

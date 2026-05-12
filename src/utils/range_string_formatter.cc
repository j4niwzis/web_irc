export module web_irc.utils.range_string_formatter;

import std;

namespace web_irc::utils {

export template <typename Range>
struct range_string_formatter {
  Range range;
};

}  // namespace web_irc::utils

template <typename Range>
struct std::formatter<web_irc::utils::range_string_formatter<Range>> {
  constexpr std::format_parse_context::iterator parse(auto& ctx) const {
    return ctx.begin();
  }

  constexpr std::format_context::iterator format(web_irc::utils::range_string_formatter<Range>& rf, auto& ctx) const {
    auto it = ctx.out();
    for(auto elem : rf.range) {
      it = std::format_to(ctx.out(), "{}", elem);
    }
    return it;
  }
  /*
  constexpr auto format( rf, ctx) const {
    return 
  }
  */
};

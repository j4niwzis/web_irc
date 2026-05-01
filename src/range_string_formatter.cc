export module web_irc.range_string_formatter;

import std;

namespace web_irc {

export template <typename Range>
struct range_string_formatter {
  Range range;
};

}  // namespace web_irc

template <typename Range>
struct std::formatter<web_irc::range_string_formatter<Range>> {
  constexpr std::format_parse_context::iterator parse(std::format_parse_context& ctx) const {
    return ctx.begin();
  }

  constexpr std::format_context::iterator format(web_irc::range_string_formatter<Range>& rf, std::format_context& ctx) const {
    for(auto elem : rf.range) {
      *ctx.out() = elem;
    }
    return ctx.out();
  }
};

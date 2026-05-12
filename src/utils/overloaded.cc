export module web_irc.utils.overloaded;

namespace web_irc::utils {

export template <typename... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};

}  // namespace web_irc::utils

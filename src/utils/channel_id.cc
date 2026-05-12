export module web_irc.utils.channel_id;
import std;

namespace web_irc::utils {

export std::string_view channel_id(std::string_view ch) {
  if(!ch.empty() && ch[0] == '#')
    return ch.substr(1);
  return ch;
}

}  // namespace web_irc::utils

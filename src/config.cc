export module web_irc.config;
import std;

namespace web_irc {

export struct server_config {
  std::string_view address;
  std::uint16_t port;
};

export constexpr struct {
  std::string_view name;
  std::string_view short_name;
  std::string_view version;
  std::string_view copyright;
  std::string_view url;
  std::string_view realname;  // for irc
  std::string_view default_channels;
  server_config connection;
  server_config irc_server;
} config
#include "../config.inc"
    ;

}  // namespace web_irc

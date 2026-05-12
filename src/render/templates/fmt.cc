export module web_irc.render.templates.fmt;
import std;

namespace web_irc::render::fmt {

constexpr auto to_array(auto... args) {
  return std::to_array<char>({static_cast<char>(args)...});
}

export constexpr auto channel_rules = to_array(
#embed "../templates/channel_rules.css.fmt"
    , 0);

export constexpr auto page_begin = to_array(
#embed "../templates/page.html.fmt"
    , 0);

export constexpr auto tab = to_array(
#embed "../templates/tab.html.fmt"
    , 0);

export constexpr auto topic = to_array(
#embed "../templates/topic.html.fmt"
    , 0);

export constexpr auto message = to_array(
#embed "../templates/message.html.fmt"
    , 0);

export constexpr auto user = to_array(
#embed "../templates/user.html.fmt"
    , 0);

export constexpr auto user_hide = to_array(
#embed "../templates/user_hide.html.fmt"
    , 0);

export constexpr auto user_show = to_array(
#embed "../templates/user_show.html.fmt"
    , 0);

export constexpr auto submit_form = to_array(
#embed "../templates/submit_form.html.fmt"
    , 0);

export constexpr auto connect_form = to_array(
#embed "../templates/connect_form.html.fmt"
    , 0);

export constexpr auto composer_frame = to_array(
#embed "../templates/composer_frame.html.fmt"
    , 0);

export constexpr auto bottom_anchor = to_array(
#embed "../templates/bottom_anchor.html.fmt"
    , 0);

}  // namespace web_irc::render::fmt

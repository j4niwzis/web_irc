# Web IRC

A truly monstrous async IRC client using **no JavaScript whatsoever** on the frontend.

## Wait, what?

This is a fully functional multi-channel IRC client that sends and receives messages in real time with no page reloads and no JavaScript.

## OK, so how?

HTTP chunked streaming + Declarative Shadow DOM + CSS `:target` tab switching + iframe forms + dynamically injected CSS rules. One persistent HTTP connection per client.

## Say that again?

The frontend world has become so addicted to JavaScript that we've stopped noticing what HTML and CSS can do with a pinch of server-side magic. HTTP streaming has existed for ages, and CSS has evolved to the point where it can handle state logic — yet we keep shipping megabytes of JavaScript (and sometimes WebAssembly) for things that can be solved differently.

The idea of a JavaScript-free IRC client isn't entirely new. Back in the early 2000s, **CGI:IRC** existed — a real IRC client that worked entirely without JavaScript, letting people chat in real time even when JS was disabled. But that was the era of table layouts, before CSS had evolved into what it is today. Modern browsers give us far more power, and we're going to abuse all of it.

This isn't just about HTTP streaming — that's just the foundation. This is about combining a **ridiculous number** of HTML/CSS tricks that browsers let us get away with. The result is a functional IRC client featuring:
- Dynamic creation of new channels
- Real-time message updates without reloading
- Sending messages
- Connection loss notifications
- Dynamically updated user lists per channel

And yes — only **one** persistent connection per client.

---

## The Foundation: HTTP Streaming

Messages arrive via HTTP streaming. The concept is simple: the connection never closes. The server keeps appending new HTML blocks using `Transfer-Encoding: chunked`. This works, but it has limitations. You can't modify HTML/CSS that has already arrived. But even with that restriction, building a full IRC client is absolutely possible.

The layout we need is straightforward: a chat area on the left, a user list on the right, and channel selector buttons at the top that hide old data and show new data.

We could consider different architectures. For example, separate iframes with persistent connections for each channel — one for messages, one for users, buttons to swap visibility. But browsers limit simultaneous connections per domain (usually 6–8), and each connection needs separate monitoring, increasing server load.

What if we use a constant number of connections across multiple channels? Two iframes — one for messages, one for users. But here's the problem: without JavaScript, you can't communicate the "active button" state into an iframe from the outside. The server could track which channel the user is viewing, but that means the server knows more than it should, and we're back to multiple persistent connections.

There's a third option. It looks like a cheat code. And since this is abnormal programming, let's use it.

**One connection. One HTML stream. Messages and users mixed together. Separate scrolling.**

Normally, separate scrolling would require two full separate blocks. But we can use "virtual" blocks with **Declarative Shadow DOM**.

| Approach | Problems |
|----------|----------|
| Multiple iframes per channel | Browser connection limits (6-8), separate monitoring per connection, server load |
| Two iframes (messages + users) | Can't communicate active tab state without JS |
| Two iframes + server tracking | Server knows too much, multiple persistent connections |
| **Declarative Shadow DOM** | **No problems** |

---

## Declarative Shadow DOM

**Declarative Shadow DOM** lets you define shadow DOM structure directly in HTML via `<template shadowrootmode="open">`, which enables slot-based content distribution. Content from the main DOM automatically flows into matching `<slot>` elements inside the shadow DOM — no JavaScript required.

Here's how it looks in `templates/page.html.fmt`:

```html
<web-client>
  <template shadowrootmode="open">
    <link rel="stylesheet" href="/static/shadow.css">

    <div class="chrome"><slot></slot></div>

    <div class="columns">
      <div class="messages-col">
        <div class="messages-scroll"><slot name="messages"></slot></div>
        <div class="composer"><slot name="composer"></slot></div>
      </div>

      <div class="users-col"><slot name="users"></slot></div>
    </div>
  </template>

  <input type="checkbox" id="users-toggle" class="inject-state">
  <label class="burger-btn" for="users-toggle">☰ Users</label>
```

And the shadow layout in `static/shadow.css`:

```css
:host {
  display: flex;
  flex-direction: column;
  width: 100%;
  height: 100%;
  overflow: hidden;
}

.columns {
  display: flex;
  flex: 1;
  min-height: 0;
}

.messages-col {
  flex: 1;
  display: flex;
  flex-direction: column;
  min-width: 0;
  min-height: 0;
}

.messages-scroll {
  flex: 1;
  min-height: 0;
  overflow-y: auto;
  display: flex;
  flex-direction: column;
}

.users-col {
  flex-shrink: 0;
  display: flex;
  flex-direction: column;
  overflow-y: auto;
  min-height: 0;
}

slot {
  display: contents;
}
```

Messages and users arrive in the light DOM with slot attributes, and the browser sorts them into the correct columns automatically:

```html
<div slot="messages" class="inject-item-page-element-general">
  [12:34:56] &lt;<span class="author">alice</span>&gt; hello
</div>
<div slot="users" class="inject-users-general">alice</div>
<div slot="composer" class="inject-frame-general" src="..."></div>
```

Separate scrolling works out of the box. Since **HTML itself distributes everything into the right buckets**, we simply append new chunks into a single stream — exactly what HTTP streaming needs.

---

## Channel Switching

With the foundation laid, we need to switch channels.

One option is radio buttons. Hidden `<input type="radio">` elements tied to `<label>` tags, with `:checked ~ .content` selectors showing the right content. But this has a deal-breaker: you can't auto-scroll to the bottom of a channel when clicking its tab. Manual scrolling every time.

The alternative — and the one we use — is **anchor links** (`href="#id"`) combined with CSS `:has(:target)`.

At the bottom of each channel, we place a hidden anchor. When you click a tab link, the browser auto-scrolls to that anchor. To ensure the anchor is always last in the flex container (even as new messages stream in), `templates/bottom_anchor.html.fmt` gives it:

```html
<div slot="messages" id="tab-general" class="bottom-anchor jump-general" aria-hidden="true"></div>
```

And `static/client.css` sets:

```css
.bottom-anchor {
  display: none;
  order: 2147483647;
  height: 1px;
  min-height: 1px;
  padding: 0 !important;
  margin: 0;
  line-height: 0;
  overflow: hidden;
  color: transparent;
  font-size: 0;
}
```

The `order: 2147483647` ensures the anchor is always at the end of the flex container, even when new messages are appended after it via streaming.

Here's the visibility trick. By default, ALL channel content is hidden in `static/client.css`:

```css
[class*="inject-item-page-element-"],
[class*="inject-users-"] {
  display: none;
}

[class*="inject-frame-"] {
  display: none;
}
```

When a tab becomes the `:target`, `templates/channel_rules.css.fmt` streams rules that override it for that specific channel:

```html
<style>
  web-client:has(#tab-general:target) .inject-item-page-element-general,
  web-client:has(#tab-general:target) .inject-users-general {
    display: block;
  }

  web-client:has(#tab-general:target) .inject-frame-general {
    display: block;
  }

  web-client:has(#tab-general:target) .inject-tab-general {
    background: #666; border-color: #777; color: #fff; font-weight: bold;
  }

  web-client:has(#tab-general:target) #tab-general {
    display: block;
  }

  web-client:has(#tab-general:target) .jump-general {
    display: block;
  }
</style>
```

The `channel_rules.css.fmt` template generates these rules dynamically for each new channel. The key insight: because HTTP streaming only appends, all CSS for a channel must be streamed **after** the shadow template and **before** the channel content.

And the default state when no tab is selected (from `static/client.css`):

```css
web-client:not(:has(:target)) .status-inject-item-page-element,
web-client:not(:has(:target)) .status-users { display: block; }

web-client:not(:has(:target)) .status-frame { display: block; }

web-client:not(:has(:target)) .status-tab {
  background: #666; border-color: #777; color: #fff; font-weight: bold;
}
```

This shows the "Status" tab by default until the user clicks another channel.

---

## Sending Messages

Receiving and displaying messages is solved. Now, how do we send?

Fortunately, HTML has plain old `<form>` elements, which work just fine without JavaScript. (Without them, we'd have to invent something like [css-only-chat](https://github.com/kkuchta/css-only-chat).)

But naive forms have a UX problem: the text doesn't clear after sending. The user has to manually delete it. To fix this, we submit the form into an **iframe**, and the server responds with the same page but a clean input field — effectively reloading just the iframe.

Each channel gets a composer iframe streamed from `templates/composer_frame.html.fmt`:

```html
<iframe slot="composer" class="inject-frame-general"
        src="/submit_form?channel=general&amp;connection_id=uuid"
        style="border:none;width:100%;height:36px;background:transparent;"
        scrolling="no"></iframe>
```

The iframe loads a standalone form page from `templates/form_page.html.fmt`:

```html
<form action="/send_message?channel=general" method="post" target="_self">
  <input type="hidden" name="connection_id" value="uuid">
  <input type="text" name="message" placeholder="Message for #general" autocomplete="off">
  <button type="submit">Send</button>
</form>
```

On submit, the server sends the message to IRC, then redirects back to a fresh form with an empty input. The main chat stream is never interrupted. There's still a minor issue: focus is lost on submit, so the user has to press Tab to get back. But that's one keystroke versus manual text deletion, so we live with it.

---

## Hiding and Showing Users

HTTP streaming means we can never remove elements from the DOM — only append. But we **can** hide them.

Each user gets a unique class generated by `templates/user.html.fmt`:

```html
<div slot="users" class="inject-users-general user-tab-general-alice">alice</div>
```

To hide a user, the server streams `templates/user_hide.html.fmt`:

```html
<style>web-client:has(#tab-general:target) .user-tab-general-alice{display:none !important}</style>
```

To show them again, it streams `templates/user_show.html.fmt`:

```html
<style>web-client:has(#tab-general:target) .user-tab-general-alice{display:initial !important}</style>
```

The server tracks which users are in which channels and streams hide/show rules as needed. The DOM grows, but the CSS keeps the display clean.

---

## Connection Status Without JavaScript

In a normal HTTP streaming setup, the browser shows an infinite loading spinner that disappears when the connection drops. But that's not very noticeable, and it vanishes entirely if the page is inside an iframe.

Our solution: a fixed-position iframe from `templates/page.html.fmt`:

```html
<iframe src="/connection_status?connection_id=uuid"
        style="position:fixed;top:50%;left:50%;transform:translate(-50%,-50%);
               width:300px;height:120px;border:none;z-index:1000;
               background:transparent;pointer-events:none;"></iframe>
```

If connected, the server returns `templates/connection_status_ok.html`:

```html
<meta http-equiv="refresh" content="30">
```

It refreshes every 30 seconds. If the IRC connection drops, the server returns `templates/connection_status_err.html`:

```html
<div class="status-box">Connection to IRC server lost</div>
```

When the entire HTTP stream collapses, the iframe shows a white rectangle — a crude but effective indicator.

---

## Heartbeat: Keeping the Stream Alive

Some proxies and browsers kill idle HTTP connections. Every 30 seconds, the server sends:

```html
  
```

Just two spaces. The browser ignores them. But they keep the TCP connection warm and the chunked stream flowing.

---

## Mention Highlighting and Beep

When someone says your nickname, the server adds a `mention` class. From `templates/message.html.fmt`:

```html
<div slot="messages" class="inject-item-page-element-general">
  [12:34:56] &lt;<span class="author mention">joe</span>&gt; hey alice
</div>
```

And `static/client.css` styles it:

```css
.author.mention {
  color: #ff6b6b;
  font-weight: bold;
}
```

And the server streams `templates/beep.html`:

```html
<audio autoplay src="/static/beep.wav"></audio>
```

No JavaScript event handlers. The browser handles it.

---

## Mobile User Drawer Without JS

Mobile needs a slide-out user list. We use the checkbox hack from `templates/page.html.fmt`:

```html
<input type="checkbox" id="users-toggle" class="inject-state">
<label class="burger-btn" for="users-toggle">☰ Users</label>
```

Clicking the label toggles the checkbox. CSS in `static/client.css` does the rest:

```css
web-client:has(#users-toggle:checked) {
  --show-users: flex;
}
```

And `static/shadow.css` uses the custom property:

```css
.users-col {
  display: var(--show-users, none);
}
```

On desktop, `--show-users` is never set. On mobile (`@media (max-width: 640px)`), the drawer is absolutely positioned and only appears when checked.

---

## Limitations

Let's be honest about the trade-offs:

- **Focus resets on send** — after submitting a message, the input loses focus. The user must press Tab to return.
- **Iframe flicker on submit** — the composer iframe reloads, causing a brief flash.
- **Connection loss indicator is crude** — a white rectangle in the center of the screen when everything dies.
- **DOM never shrinks** — old messages and users can't be removed, so memory usage grows forever.
- **Modern browser required** — `:has()` and Declarative Shadow DOM are relatively new. Old browsers and text-mode clients need not apply.
- **Weak machines may struggle** — an ever-growing DOM eventually suffocates low-end hardware.

---

## Why This Matters

We've become so accustomed to the idea that browser dynamics === JavaScript that we stopped noticing: the HTML parser, the CSS engine, and one persistent HTTP connection already contain enormous potential for declarative logic.

This project is valuable precisely as **research on the edge of possibility**. It tests how much application logic can be pushed into the platform layers we usually ignore. Every trick used here — chunked streaming, `:has(:target)`, Declarative Shadow DOM slots, iframe form targets, dynamically injected CSS — is a standard web platform feature. None of them are hacks in the sense of exploiting browser bugs. They are hacks only in the sense that we combined them in a way the platform designers probably didn't anticipate.

The boundaries between web platform layers keep shifting. `:has()` arrived in 2022–2023. Declarative Shadow DOM arrived even more recently. A decade ago, this client would have been impossible. A decade from now, who knows what else will become possible without scripts?

This client isn't a call to abandon JS in all projects. It's a reminder that sometimes 80% of a problem can be solved without a single `<script>` tag, and that re-examining our assumptions about what "requires JavaScript" often reveals simpler, more robust alternatives hiding in plain sight.

---

## Configuration

Before building, create `config.inc` from `config.inc.example`:

```bash
cp config.inc.example config.inc
```

`config.inc` is a C++20 designated-initializer file compiled into the binary. It contains deployment-specific settings:

```cpp
{
  .name = "Web IRC",
  .short_name = "web-irc",
  .default_channels = "#test",
  .connection = {
    .address = "0.0.0.0",   // HTTP bind address
    .port = 8080            // HTTP port
  },
  .irc_server = {
    .address = "127.0.0.1", // IRC server address
    .port = 6667            // IRC server port
  },
  .webirc = webirc::use{
    .password = "webirc_password",
    .gateway = "irc.local",
    .real_ip_from_headers = true  // Use X-Forwarded-For / X-Real-IP
  }
}
```

You do **not** need to include `config.inc` in source distributions. See `LICENSE.addon`.

## Building

Requires:
- CMake >= 4.1.1
- A C++26 compiler with modules support
- Boost >= 1.88

```bash
cmake -S . -B build -GNinja
cmake --build .
```

Dependencies are fetched automatically via CPM:
- [j4niwzis/mirc](https://github.com/j4niwzis/mirc) — IRC protocol library
- [avast/asio-mutex](https://github.com/avast/asio-mutex) — Async mutexes for Asio

## Running

```bash
./build/web_irc
```

Then open `http://localhost:8080/` in your browser.

## License

Licensed under the GNU Affero General Public License, Version 3.

`LICENSE.addon` grants an additional permission under AGPLv3 section 7: you are not required to include deployment-specific configuration files (such as `config.inc`) in the Corresponding Source you distribute, provided that an example or template version (e.g., `config.inc.example`) is included so recipients can still build the program.

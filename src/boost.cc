module;
#include <async_mutex.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/system/error_code.hpp>
export module boost;

namespace boost {

namespace asio {

export using asio::awaitable;
export using asio::detached;
export using asio::use_awaitable;
export using asio::co_spawn;
export using asio::steady_timer;
export using asio::io_context;
export using asio::async_write;
export using asio::async_read_until;
export using asio::async_connect;
export using asio::streambuf;
export using asio::buffers_begin;
export using asio::buffers_end;
export using asio::buffer;
export using asio::redirect_error;
export using asio::socket_base;
export using asio::deferred;
export using asio::strand;
export using asio::any_io_executor;
export using asio::dispatch;

namespace error {

export using error::try_again;
export using error::would_block;
export using error::eof;

}  // namespace error

namespace experimental {
export using experimental::channel;
export using experimental::channel_traits;
export using experimental::make_parallel_group;
export using experimental::wait_for_one;
export using experimental::wait_for_one_error;

namespace awaitable_operators {

export using awaitable_operators::operator||;

}

}  // namespace experimental

namespace ip {
export using ip::tcp;
export using ip::address;
export using ip::make_address;
}  // namespace ip

namespace this_coro {
export using this_coro::executor;
}

namespace ssl {
export using ssl::stream;

}

}  // namespace asio

namespace system {
export using system::error_code;
export using system::system_error;

}  // namespace system

namespace beast {

export using beast::tcp_stream;
export using beast::flat_buffer;
export using beast::error_code;

namespace http {

export using http::async_write;
export using http::async_read;
export using http::async_write_header;
export using http::response;
export using http::field;
export using http::vector_body;
export using http::status;
export using http::string_body;
export using http::make_chunk;
export using http::make_chunk_last;
export using http::request;
export using http::request_parser;
export using http::verb;
export using http::empty_body;
export using http::response_serializer;

}  // namespace http

}  // namespace beast

namespace intrusive::detail {
export using detail::destructor_impl;
}

}  // namespace boost

namespace std {

export using std::coroutine_traits;

}

namespace avast::asio {

export using asio::async_mutex;
export using asio::async_mutex_lock;

}  // namespace avast::asio

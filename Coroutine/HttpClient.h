#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "Define.h"
#include "Uri.h"

enum Connection
{
	KEEP_ALIVE,
	CLOSE
};

class HttpClient
{
public:

	HttpClient(boost::asio::any_io_executor exec, boost::asio::ssl::context& ssl_context, const std::string_view url)
		: _resolver{ exec }
		, _ssl_context{ std::move(ssl_context) }
		, _ssl_stream{ exec, _ssl_context }
		, _url{ url }
	{
	}

	explicit HttpClient(boost::asio::any_io_executor exec, const std::string_view url)
		: _resolver{ exec }
		, _ssl_context{ boost::asio::ssl::context{boost::asio::ssl::context::tlsv13_client} }
		, _ssl_stream{ exec, _ssl_context }
		, _url{ url }
	{
	}

	~HttpClient()
	{
		std::cout << "HttpClient disconnect and destroyed\n";

		boost::system::error_code ec;
		_ssl_stream.lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
	}

	template <typename T>
	boost::asio::awaitable<boost::beast::http::response<T>> get(const std::string_view target, const Headers& headers = {}, const std::chrono::seconds& timeout = std::chrono::seconds(30))
	{
		co_return co_await request<T>(target, Verb::get, {}, "", headers, timeout);
	}

	template <typename T>
	boost::asio::awaitable<boost::beast::http::response<T>> post(const std::string_view target, const Body& body, const std::string& content_type = "application/json", const Headers& headers = {}, const std::chrono::seconds& timeout = std::chrono::seconds(30))
	{
		co_return co_await request<T>(target, Verb::post, body, content_type, headers, timeout);
	}

	template <typename T>
	boost::asio::awaitable<boost::beast::http::response<T>> put(const std::string_view target, const Body& body, const std::string& content_type = "application/json", const Headers& headers = {}, const std::chrono::seconds& timeout = std::chrono::seconds(30))
	{
		co_return co_await request<T>(target, Verb::put, body, content_type, headers, timeout);
	}

	template <typename T>
	boost::asio::awaitable<boost::beast::http::response<T>> head(const std::string_view target, const Headers& headers = {}, const std::chrono::seconds& timeout = std::chrono::seconds(30))
	{
		co_return co_await request<T>(target, Verb::head, {}, "", headers, timeout);
	}

	boost::asio::awaitable<void> connect(const Connection& con_type = Connection::KEEP_ALIVE, const std::chrono::seconds& timeout = std::chrono::seconds(30))
	{
		const auto protocol = _url.scheme().empty() ? "http" : _url.scheme();
		const auto port = _url.port();
		_keep_alive = con_type;

		if (protocol != "http" && protocol != "https")
		{
			throw std::runtime_error{ "Unsupported protocol: " + std::string(protocol) };
		}

		_ssl_stream.next_layer().expires_after(timeout);
		auto [ec, results] = co_await _resolver.async_resolve(_url.host(), std::to_string(port), boost::asio::as_tuple(boost::asio::use_awaitable));
		if (ec)
		{
			throw std::runtime_error(ec.message());
		}

		if (protocol == "https")
		{
			// Set SNI Hostname (many hosts need this to handshake successfully)
			if (!SSL_set_tlsext_host_name(_ssl_stream.native_handle(), _url.host().data()))
			{
				ec = boost::beast::error_code{ static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
				std::cerr << ec.message() << "\n";
				throw std::runtime_error(ec.message());
			}

			_ssl_stream.next_layer().socket().open(boost::asio::ip::tcp::v4());
			co_await _ssl_stream.next_layer().async_connect(results, boost::asio::use_awaitable);
			co_await _ssl_stream.async_handshake(boost::asio::ssl::stream_base::client, boost::asio::use_awaitable);
		}
		else
		{
			co_await _ssl_stream.next_layer().async_connect(results, boost::asio::use_awaitable);
		}
	}

private:
	template <typename T>
	boost::asio::awaitable<boost::beast::http::response<T>> request(const std::string_view target_, const Verb& verb, const Body& body = {}, const std::string& content_type = "", const Headers& headers = {}, const std::chrono::seconds& timeout = std::chrono::seconds(30))
	{
		try
		{
			uri _target(target_);
			boost::beast::http::request<boost::beast::http::string_body> req{ verb, _target.target(), 11 };
			{
				req.set(boost::beast::http::field::host, _url.host());
				req.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);
				req.set(boost::beast::http::field::content_type, content_type);

				// if keep-alive false set connection close
				if (_keep_alive == Connection::CLOSE)
					req.set(boost::beast::http::field::connection, "close");

				for (const auto& val : headers)
					req.set(val.first, val.second);

				// set body if specified
				if (!body.empty())
				{
					req.body() = std::string(body.begin(), body.end());
					req.content_length(body.size());
				}
			}

			std::cout << "Send request to: " << _url.str() << " " << target_ << "\n" << req;
			if (_url.scheme() == "https")
				co_await boost::beast::http::async_write(_ssl_stream, req, boost::asio::use_awaitable);
			else
				co_await boost::beast::http::async_write(_ssl_stream.next_layer(), req, boost::asio::use_awaitable);

			boost::beast::flat_buffer buffer;
			boost::beast::http::response<T> res;
			if (_url.scheme() == "https")
				co_await boost::beast::http::async_read(_ssl_stream, buffer, res, boost::asio::use_awaitable);
			else
				co_await boost::beast::http::async_read(_ssl_stream.next_layer(), buffer, res, boost::asio::use_awaitable);

			co_return res;
		}
		catch (const std::exception& e)
		{
			std::cerr << "Error: " << e.what() << std::endl;
			co_return boost::beast::http::response<T>(Status::internal_server_error, 11);
		}
	}

	static std::chrono::system_clock::time_point seconds_to_time_point(const std::chrono::seconds& duration)
	{
		return std::chrono::system_clock::now() + duration;
	}

	static std::string to_http_date(const std::chrono::seconds& seconds)
	{
		std::time_t time = std::chrono::system_clock::to_time_t(seconds_to_time_point(seconds));
		std::stringstream ss;
		ss << std::put_time(std::gmtime(&time), "%a, %d %b %Y %H:%M:%S GMT");
		return ss.str();
	}

private:
	boost::asio::ip::tcp::resolver _resolver;
	boost::asio::ssl::context _ssl_context{ boost::asio::ssl::context::sslv23_client };
	boost::asio::ssl::stream<boost::beast::tcp_stream> _ssl_stream;
	const uri _url;
	Connection _keep_alive;
};
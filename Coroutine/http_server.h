#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>
#include <boost/function.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "Api.h"

#include <mutex>
#include <map>
#include <iostream>

class HttpServer
{
	using StoredApi = std::function<boost::asio::awaitable<boost::beast::http::message_generator>(const Request& req)>;
private:
	// Report a failure
	void fail(boost::system::error_code ec, std::string what)
	{
		throw std::runtime_error(what + ": " + ec.message());
	}
public:
	~HttpServer()
	{
	}
	HttpServer(boost::asio::any_io_executor exec, boost::asio::ip::tcp::endpoint endpoint)
		: _exec{ exec }
		, _ep{ endpoint }
	{
	}

	template<IApi T>
	void AddApi(T& api)
	{
		_apis.push_back([&api](const Request& req) {
			return api.HandleRequest(req);
		});
	}

	boost::asio::awaitable<void> DoAccept()
	{

		auto acceptor = boost::asio::use_awaitable.as_default_on(boost::asio::ip::tcp::acceptor(co_await boost::asio::this_coro::executor));
		acceptor.open(_ep.protocol());

		acceptor.set_option(boost::asio::socket_base::reuse_address(true));
		acceptor.bind(_ep);

		acceptor.listen(boost::asio::socket_base::max_listen_connections);
		std::cout << "Http server running at: " << _ep.address().to_string() << ":" << _ep.port() << "\n";
		std::cout << "Awaiting connection...\n";

		for (;;)
		{
			boost::asio::co_spawn(_exec, OnAccept(boost::beast::tcp_stream(co_await acceptor.async_accept())), [](std::exception_ptr e)
			{
				if (e)
				try
				{
					std::rethrow_exception(e);
				}
				catch (const std::exception& e)
				{
					std::cerr << "Error in session: " << e.what() << "\n";
				}
			});
		}
	}

	boost::asio::awaitable<void> OnAccept(boost::beast::tcp_stream stream)
	{
		const std::string stream_ip = stream.socket().remote_endpoint().address().to_string();
		std::cout << "New connection accepted from: " << stream_ip << "\n";

		for (;;)
		{
			try
			{
				Request req;
				boost::beast::flat_buffer buffer;

				// set expiration timer
				stream.expires_after(std::chrono::seconds(30));

				auto [error_read, readed_bytes] = co_await boost::beast::http::async_read(stream, buffer, req, boost::asio::as_tuple(boost::asio::use_awaitable));
				// handle socket timeout or connection lost ?
				if (error_read && error_read.value() == (int)boost::beast::http::error::end_of_stream)
				{
					std::cout << "Connection lost to: " << stream_ip << "\n";
					co_return;
				}

				// timeout test on write
				std::this_thread::sleep_for(std::chrono::seconds(5));

				// this code is temporary till i manage collection of request by api object (POST /v1/create_resources) etc
				for (auto& api : _apis)
				{
					// need to give api() a res to fill and return a boolean if the request has been processed or not in order to give it to the next api or to return a default 404
					boost::beast::http::message_generator msg = co_await api(req);
					auto [error_write, sent_bytes] =  co_await boost::beast::async_write(stream, std::move(msg), boost::asio::as_tuple(boost::asio::use_awaitable));
					if (error_write && error_write.value() == boost::asio::error::connection_aborted)
					{
						std::cout << "Connection lost to: " << stream_ip << "\n";
						co_return;
					}
				}

				if (!req.keep_alive())
				{
					break;
				}
			}
			catch (boost::system::system_error& se)
			{
				if (se.code() != boost::beast::http::error::end_of_stream)
				{
					if (se.code() != boost::beast::errc::connection_aborted && se.code() != boost::beast::errc::connection_reset && se.code() != boost::beast::errc::operation_canceled)
					{
						std::cerr << "Error in OnAccept: " << se.code() << " " << se.what() << "\n";
						throw;
					}
				}
			}
		}

		std::cout << "Connection closed: " << stream_ip << "\n";
		boost::system::error_code ec;
		stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
	}

private:
	boost::asio::any_io_executor _exec;
	boost::asio::ip::tcp::endpoint _ep;
	std::list<StoredApi> _apis;
};

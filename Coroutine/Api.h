#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http/message.hpp>

#include <concepts>
#include <list>
#include <queue>

#include "Define.h"
#include "MiddleWare.h"

struct APIEntry
{
    std::string uri;
    Verb verb;
    std::function<boost::asio::awaitable<boost::beast::http::message_generator>(const Request& req)> fnct;
};

template <typename T>
concept IApi = requires(T api, const Request & req)
{
	{ api.HandleRequest(req) } -> std::convertible_to<boost::asio::awaitable<boost::beast::http::message_generator>>;
};

/// <summary>
/// Define an Api
/// </summary>
class Api final
{
    using StoredMiddleware = std::function<boost::asio::awaitable<bool>(const Request& req, Status& code, std::string& err)>;
public:
    Api()
        // can also use regex to match specific data..
        : _entries { APIEntry{"/toto", Verb::post, std::bind(&Api::HandlePostToto, this, std::placeholders::_1)}, 
                     APIEntry{"/", Verb::get, std::bind(&Api::HandleGet, this, std::placeholders::_1)}}
    {

    }

    /// <summary>
    /// Push a middleware queue to process before the request threatement
    /// </summary>
    /// <param name="middleware"></param>
    template <IMiddleWare T>
    void AddMiddleWare(T& middleware)
    {
        _middleware.push_back([&middleware](const Request& req, Status& code, std::string& err) {
            return middleware.HandleRequest(req, code, err);
        });
    }

    /// <summary>
    /// Handle a request
    /// </summary>
    /// <param name="req"></param>
    /// <returns></returns>
    boost::asio::awaitable<boost::beast::http::message_generator> HandleRequest(const Request& req) const 
    {
        std::string err;
        Status code;

        for (auto& middleware : _middleware)
        {
            const auto& success = co_await middleware(req, code, err);
            if (!success)
            {
                auto res = co_await GenerateResponse(req, code, err);
                co_return res;
            }
        }

        for (auto& entry : _entries)
        {
            if (entry.uri == req.get().target() && entry.verb == req.get().method())
            {
                auto res = co_await entry.fnct(req);
                co_return res;
            }
        }

        code = Status::not_found;
        co_return co_await GenerateResponse(req, code, err);
    }

private:

    boost::asio::awaitable<boost::beast::http::message_generator> HandlePostToto(const Request& req) const
    {
        Status code = Status::created;

        const std::string body("\"Hello World!\"");
        auto res = co_await GenerateResponse(req, code, body);
        co_return res;
    }

    boost::asio::awaitable<boost::beast::http::message_generator> HandleGet(const Request& req) const
    {
        Status code = Status::ok;

        const std::string body("\"Hello World!\"");
        auto res = co_await GenerateResponse(req, code, body);
        co_return res;
    }

    /// <summary>
    /// Private implementation, OnError to return a error response
    /// </summary>
    /// <param name="code"></param>
    /// <param name="err"></param>
    /// <returns></returns>
    boost::asio::awaitable<boost::beast::http::message_generator> GenerateResponse(const Request& req, Status& code, const std::string& body) const
    {
        boost::beast::http::response<boost::beast::http::string_body> res{ code, req.get().version() };
        {
            res.keep_alive(req.get().keep_alive());
            res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(boost::beast::http::field::content_type, "application/json");
            res.set(boost::beast::http::field::content_length, std::to_string(body.size()));
            res.body() = body;
            res.prepare_payload();
        }

        std::cout << "Response[" << code << "] : " << body << "\n";

        co_return res;
    }

private:
    std::list<APIEntry> _entries;
    std::list<StoredMiddleware> _middleware;
};
#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http/message.hpp>

#include <concepts>

#include "Define.h"

template <typename T>
concept IMiddleWare = requires(T middleware, const Request& req, Status& code, std::string & err)
{
    { middleware.HandleRequest(req, code, err) } -> std::convertible_to<boost::asio::awaitable<bool>>;
};

/// <summary>
/// Define a logger middleware
/// </summary>
class LoggingMiddleWare
{
public:
    boost::asio::awaitable<bool> HandleRequest(const Request& req, Status& code, std::string& err)
    {
        const Body& body = req.get().body();

        std::cout << "Received request on:\n" << req.get().method() << " " << req.get().target() << "\n";

        for (auto& header : req.get())
        {
            std::cout << header.name_string() << " : " << header.value() << "\n";
        }
        if (!body.empty())
            std::cout << "Body:\n" << std::string(body.begin(), body.end()) << "\n";

        co_return true;
    }
};

/// <summary>
/// Define a token like middleware (Bearer)
/// </summary>
class TokenAuthMiddleWare
{
public:
    boost::asio::awaitable<bool> HandleRequest(const Request& req, Status& code, std::string& err)
    {
        // up to you to handle tokens or whatever as you wish to (database etc)
        std::list<std::string> tokens = {
            "Bearer toto"
        };

        code = Status::unauthorized;
        err = "\"Invalid authentification\"";

        const auto& auth = req.get().find("Authorization");
        if (auth != req.get().end())
        {
            if (tokens.end() == std::find(tokens.begin(), tokens.end(), auth->value()))
            {
                co_return false;
            }
        }
        else
            co_return false;

        co_return true;
    }
};
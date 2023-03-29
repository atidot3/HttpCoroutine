#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http/message.hpp>

#include <map>
#include <vector>

using Request = boost::beast::http::request_parser<boost::beast::http::vector_body<char>>;
using Parameters = std::map<std::string, std::string>;
using Headers = std::map<std::string, std::string>;
using Verb = boost::beast::http::verb;
using Status = boost::beast::http::status;
using Body = std::vector<char>;
using Response = boost::beast::http::message_generator;
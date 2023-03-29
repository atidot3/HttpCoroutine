#include <regex>
#include <charconv>
#include <iostream>
#include <optional>
#include <charconv>

#include "Uri.h"

//---- private impl ----------------------------------------------------

static std::regex url_rx = std::regex{ "^(?:([a-z]+):)*(?://)*([^/?#]+)*([^?#]*)([^#]*)(.*)$", std::regex_constants::ECMAScript | std::regex_constants::icase };
static std::regex authority_rx = std::regex{ "^(?:([^:@]*)(?::([^@]*))*@)*([^:]*)(?::(\\d+))*$", std::regex_constants::ECMAScript | std::regex_constants::icase };
static std::string_view default_http_scheme = "http";
static std::string_view default_path = "/";

struct uri::impl
{
    //data view only accessor
    std::string_view data_view;

    //store complete url if needed (when modifying datas)
    std::optional<std::string> data;

    //url parts
    std::string_view scheme;
    std::string_view authority;
    std::string_view user;
    std::string_view pass;
    std::string_view host;
    uint16_t port = 0;
    std::string_view path;
    std::string_view query;
    std::string_view fragment;
    std::string_view target;

    //constructor
    impl(std::string_view str, bool makeCopy)
        : data_view{ str }
    {
        if (makeCopy) make_copy();
        parse_url(data_view);
    }

    void make_copy()
    {
        data = std::string{ data_view };
        data_view = std::string_view{ data.value() };
    }

        //parse full url
        void parse_url(std::string_view str)
        {
            //std::cout << "uri: parsing url " << str << "\n";
            auto matches = std::match_results<std::string_view::const_iterator>{};

            if (std::regex_match(str.cbegin(), str.cend(), matches, url_rx))
            {
                for (size_t i = 1; i < matches.size(); i++)
                {
                    auto first = matches[i].first;
                    auto last = matches[i].second;

                    switch (i)
                    {
                    case 1: //scheme
                        scheme = std::string_view{ first, last };
                        break;
                    case 2: //authority
                        authority = std::string_view{ first, last };
                        parse_authority(authority);
                        break;
                    case 3: //path + target
                        path = std::string_view{ first, last };
                        target = std::string_view{ first, str.cend() };
                        break;
                    case 4: //query
                        query = std::string_view{ first, last };
                        break;
                    case 5: //fragments
                        fragment = std::string_view{ first, last };
                        break;
                    }
                }
            }
            else
            {
                auto msg = std::string{ "unable to parse uri: " };
                msg += str;
                throw std::invalid_argument(msg);
            }
        }

        //parse authority part only
        void parse_authority(std::string_view str)
        {
            auto matches = std::match_results<std::string_view::const_iterator>{};

            if (std::regex_match(str.cbegin(), str.cend(), matches, authority_rx))
            {
                for (size_t i = 1; i < matches.size(); i++)
                {
                    auto first = matches[i].first;
                    auto last = matches[i].second;

                    //std::cout << "parse_authority: match " << i << ": " << std::string_view{first, last} << std::endl;

                    switch (i) {
                    case 1: //user
                        user = std::string_view{ first, last };
                        break;
                    case 2: //pass
                        pass = std::string_view{ first, last };
                        break;
                    case 3: //hostname
                        host = std::string_view{ first, last };
                        break;
                    case 4: //port
                        std::string_view port_(first, last);
                        if (port_.empty()) continue;

                        auto result = std::from_chars(port_.data(), port_.data() + port_.size(), this->port);
                        if (result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range)
                        {
                            throw std::runtime_error("Invalid port specified");
                        }
                        break;
                    }
                }

                //auto set port based on scheme if not set
                if (port == 0)
                {
                    if (scheme.empty()) port = 80;
                    else if (scheme == "http") port = 80;
                    else if (scheme == "https") port = 443;
                    else if (scheme == "ftp") port = 21;
                    else if (scheme == "ssh") port = 22;
                }
            }
            else
            {
                auto msg = std::string{ "unable to parse authority: " };
                msg += str;
                throw std::invalid_argument(msg);
            }
        }

        void update_value(std::string_view& dst, std::string_view src)
        {
            //get positions for replacement
            auto dst_start = std::string::size_type(dst.data() - data_view.data());

            //copy if we are not owning the string
            if (!data.has_value()) make_copy();

            //replace data in string
            data->replace(dst_start, dst.length(), src);

            //update views
            data_view = std::string_view{ data.value() };
            parse_url(data_view);
        }

    };


    //---- public interface ----------------------------------------------------

    uri::uri(std::string_view str, bool makeCopy/*=false*/)
        : _impl{ std::make_unique<uri::impl>(str, makeCopy) }
    {
    }
    uri::~uri() = default;

    //getters

    std::string_view uri::str() const
    {
        return _impl->data_view;
    }

    std::string_view uri::scheme() const
    {
        if (_impl->scheme.empty()) return default_http_scheme;
        else return _impl->scheme;
    }

    std::string_view uri::authority() const
    {
        return _impl->authority;
    }

    std::string_view uri::user() const
    {
        return _impl->user;
    }

    std::string_view uri::pass() const
    {
        return _impl->pass;
    }

    std::string_view uri::host() const
    {
        return _impl->host;
    }

    uint16_t uri::port() const
    {
        return _impl->port;
    }

    std::string_view uri::path() const
    {
        if (_impl->path.empty()) return default_path;
        else return _impl->path;
    }

    std::string_view uri::query() const
    {
        return _impl->query;
    }

    std::string_view uri::fragment() const
    {
        return _impl->fragment;
    }

    std::string_view uri::target() const
    {
        if (_impl->target.empty()) return default_path;
        else return _impl->target;
    }

    //setters

    void uri::scheme(std::string_view scheme)
    {
        _impl->update_value(_impl->scheme, scheme);
    }

    void uri::authority(std::string_view authority)
    {
        _impl->update_value(_impl->authority, authority);
    }

    void uri::user(std::string_view user)
    {
        _impl->update_value(_impl->user, user);
    }

    void uri::pass(std::string_view pass)
    {
        _impl->update_value(_impl->pass, pass);
    }

    void uri::host(std::string_view host)
    {
        _impl->update_value(_impl->host, host);
    }

    void uri::port(uint16_t /*port*/)
    {
        //TODO: NOT IMPLEMENTED YET
    }

    void uri::path(std::string_view path)
    {
        _impl->update_value(_impl->path, path);
    }

    void uri::query(std::string_view query)
    {
        _impl->update_value(_impl->query, query);
    }

    void uri::fragment(std::string_view fragment)
    {
        _impl->update_value(_impl->fragment, fragment);
    }

    void uri::target(std::string_view target)
    {
        _impl->update_value(_impl->target, target);
    }
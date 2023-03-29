#pragma once
#include <memory>
#include <string_view>

class uri
{
    public:
    uri(std::string_view str, bool makeCopy = false);
    ~uri();

    //getters
    std::string_view str() const;
    std::string_view scheme() const;
    std::string_view authority() const;
    std::string_view user() const;
    std::string_view pass() const;
    std::string_view host() const;
    uint16_t port() const;
    std::string_view path() const;
    std::string_view query() const;
    std::string_view fragment() const;
    std::string_view target() const;

    //setters
    void scheme(std::string_view);
    void authority(std::string_view);
    void user(std::string_view);
    void pass(std::string_view);
    void host(std::string_view);
    void port(uint16_t port);
    void path(std::string_view);
    void query(std::string_view);
    void fragment(std::string_view);
    void target(std::string_view);

private:
    struct impl;
    std::unique_ptr<impl> _impl;
};
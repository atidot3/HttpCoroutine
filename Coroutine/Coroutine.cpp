#include <coroutine>
#include <iostream>
#include <stdexcept>

#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <boost/bind/bind.hpp>

#include "http_server.h"
#include "HttpClient.h"

static int count = 0;
static int success = 0;

template <class T>
void UnitTest(const boost::beast::http::response<T>& res, const Status& s)
{
    std::cout << "EXPECTED RESULT\t=> [" << s << "] RECEIVED => [" << res.result() << "]";
    if (res.result() == s)
    {
        success++;
        std::cout << " SUCCESS\n\n";
    }
    else
        std::cout << " FAILED\n\n";

    count++;
}

boost::asio::awaitable<void> DoUnitTests(boost::asio::any_io_executor exec)
{
    // let http server start
    std::this_thread::sleep_for(std::chrono::seconds(2));

	// -- Expecting full success
	try
	{
		// let http server start
		std::this_thread::sleep_for(std::chrono::seconds(2));
		HttpClient client(exec, "127.0.0.1:8080");
		co_await client.connect();

		Body body = {};
		Headers headers = { {"Authorization", "Bearer toto"} };
		auto res = co_await client.get<boost::beast::http::string_body>("/", headers);
		std::cout << "Result: " << res << "\n";
		UnitTest(res, Status::ok);

		auto r_res = co_await client.post<boost::beast::http::string_body>("/toto", body,
			"application/json", headers);
		std::cout << "Result: " << r_res << "\n";
		UnitTest(r_res, Status::created);
		std::cout << "\n";

		headers = { {"Authorization", "Bearer tata"} };
		auto t_res = co_await client.post<boost::beast::http::string_body>("/toto", body,
			"application/json", headers);
		std::cout << "Result: " << t_res << "\n";
		UnitTest(t_res, Status::unauthorized);
		std::cout << "\n";

		HttpClient https_client(exec, "https://www.google.com");
		co_await https_client.connect();
		auto e_res = co_await https_client.get<boost::beast::http::string_body>("/");
		std::cout << "Result: " << e_res << "\n";
		UnitTest(e_res, Status::ok);
		std::cout << "\n";
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << "\n";
	}

	// -- Expecting failed
	try
	{
		HttpClient https_client_2(exec, "https://www.invalid-url-that-doesnt-exists.com");
		co_await https_client_2.connect();
		count++;
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}
    
	std::cout << "\n\tResult: " << count << " tests => " << success << " SUCCESS " << count - success << " FAILED\n\n";

	co_return;
}

int main()
{
    boost::asio::thread_pool pool{ 8 };

    try
    {
        // install sighandlers
        boost::asio::signal_set signals(pool, SIGINT, SIGTERM);
        signals.async_wait(boost::bind(&boost::asio::thread_pool::stop, &pool));

        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 8080);
        HttpServer server(pool.get_executor(), endpoint);
        Api api;

        server.AddApi(api);

        auto log = LoggingMiddleWare();
        auto auth = TokenAuthMiddleWare();
        api.AddMiddleWare(log);
        api.AddMiddleWare(auth);

        boost::asio::co_spawn(pool, server.DoAccept(), boost::asio::detached);
        boost::asio::co_spawn(pool, DoUnitTests(pool.get_executor()), boost::asio::detached);

        pool.join();
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        pool.join();
    }

    return 0;
}
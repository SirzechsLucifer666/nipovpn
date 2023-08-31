#ifndef SERVER_HPP
#define SERVER_HPP

#include "config.hpp"
#include "log.hpp"
#include "header.hpp"
#include "request.hpp"
#include "response.hpp"
#include "session.hpp"

using namespace std;

class server {
	public:
		server(const server&) = delete;
		server& operator=(const server&) = delete;
		explicit server(Config config);
		void run();
		Config nipoConfig;
		Log nipoLog;
	
	private:
		void doAccept();
		void doAwaitStop();
		boost::asio::io_context io_context_;
		boost::asio::signal_set signals_;
		boost::asio::ip::tcp::acceptor acceptor_;
		SessionManager SessionManager_;
		RequestHandler RequestHandler_;
};

#endif // SERVER_HPP

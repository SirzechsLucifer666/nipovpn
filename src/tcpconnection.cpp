#include "tcpconnection.hpp"

/*
* TCPConnection
*/
TCPConnection::TCPConnection(boost::asio::io_context& io_context, 
	const std::shared_ptr<Config>& config, 
	const std::shared_ptr<Log>& log,
	const TCPClient::pointer& client)
	: socket_(io_context),
		config_(config),
		log_(log),
		io_context_(io_context),
		client_(client)
{ }

boost::asio::ip::tcp::socket& TCPConnection::socket()
{
	return socket_;
}

void TCPConnection::start()
{
	doRead();
}

void TCPConnection::doRead()
{
	try
	{
		readBuffer_.consume(readBuffer_.size());
		boost::asio::async_read(
		socket_,
		readBuffer_,
		boost::asio::transfer_at_least(1),
		boost::bind(&TCPConnection::handleRead,
			shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
		);
	}
	catch (std::exception& error)
	{
		log_->write(std::string("[TCPConnection doRead] [catch] ") + error.what(), Log::Level::ERROR);
	}
}

void TCPConnection::handleRead(
	const boost::system::error_code& error,
	size_t bytes_transferred)
{
	try
	{
		boost::system::error_code error;
		if (socket_.available() > 0)
		{
			while(true)
			{
				if (socket_.available() == 0)
					break;
				auto size = boost::asio::read(
					socket_,
					readBuffer_,
					boost::asio::transfer_at_least(1),
					error
				);
				if (error == boost::asio::error::eof || size == 0)
					break;
				else if (error)
				{
					log_->write(std::string("[TCPConnection handleRead] [error] ") + error.what(), Log::Level::ERROR);
				}
			}
		}
		if (readBuffer_.size() > 0)
		{
			try
			{
				log_->write("[TCPConnection handleRead] [SRC " +
					socket_.remote_endpoint().address().to_string() +":"+
					std::to_string(socket_.remote_endpoint().port())+"] [Bytes "+
					std::to_string(readBuffer_.size())+"] ",
					Log::Level::INFO);
			}
			catch (std::exception& error)
			{
				log_->write(std::string("[TCPConnection handleRead] [catch] ") + error.what(), Log::Level::ERROR);
			}
			if (config_->runMode() == RunMode::agent)
			{
				AgentHandler::pointer agentHandler_ = AgentHandler::create(readBuffer_, writeBuffer_, config_, log_, client_);
				agentHandler_->handle();
				doWrite();
			} else if (config_->runMode() == RunMode::server)
			{
				ServerHandler::pointer serverHandler_ = ServerHandler::create(readBuffer_, writeBuffer_, config_, log_,  client_);
				serverHandler_->handle();
				doWrite();
			}
		}
		doRead();
	}
	catch (std::exception& error)
	{
		log_->write(std::string("[TCPConnection handleRead] [catch] ") + error.what(), Log::Level::ERROR);
	}
}

void TCPConnection::doWrite()
{
	try
	{
		log_->write("[TCPConnection doWrite] [DST " + 
			socket_.remote_endpoint().address().to_string() +":"+ 
			std::to_string(socket_.remote_endpoint().port())+"] [Bytes " +
			std::to_string(writeBuffer_.size())+"] ", 
			Log::Level::DEBUG);
		boost::system::error_code error;
		boost::asio::write(
			socket_,
			writeBuffer_,
			error
		);
		if (error)
		{
			log_->write(std::string("[TCPConnection doWrite] [error] ") + error.what(), Log::Level::ERROR);
		}
	}
	catch (std::exception& error)
	{
		log_->write(std::string("[TCPConnection doWrite] [catch] ") + error.what(), Log::Level::ERROR);
	}
}
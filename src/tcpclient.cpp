#include "tcpclient.hpp"

/*
* TCPClient
*/
TCPClient::TCPClient(boost::asio::io_context& io_context, 
	const std::shared_ptr<Config>& config, 
	const std::shared_ptr<Log>& log)
	: config_(config),
		log_(log),
		io_context_(io_context),
		socket_(io_context),
		resolver_(io_context)
{ }

boost::asio::ip::tcp::socket& TCPClient::socket()
{
	return socket_;
}

void TCPClient::writeBuffer(boost::asio::streambuf& buffer)
{
	moveStreamBuff(buffer, writeBuffer_);
}

void TCPClient::doConnect()
{
	try
	{
		log_->write("[TCPClient doConnect] [DST] " +
			config_->agent().serverIp +":"+
			std::to_string(config_->agent().serverPort)+" ",
			Log::Level::DEBUG);
	
		boost::asio::ip::tcp::resolver resolver(io_context_);
		auto endpoint = resolver.resolve(config_->agent().serverIp.c_str(), std::to_string(config_->agent().serverPort).c_str());
		boost::asio::connect(socket_, endpoint);
		boost::asio::socket_base::keep_alive option(true);
		socket_.set_option(option);
	}
	catch (std::exception& error)
	{
		log_->write(std::string("[TCPClient doConnect] ") + error.what(), Log::Level::ERROR);
	}
}

void TCPClient::doConnect(const std::string& dstIP, const unsigned short& dstPort)
{
	try
	{
		log_->write("[TCPClient doConnect] [DST] " +
			dstIP +":"+
			std::to_string(dstPort)+" ",
			Log::Level::DEBUG);
	
		boost::asio::ip::tcp::resolver resolver(io_context_);
		auto endpoint = resolver.resolve(dstIP.c_str(), std::to_string(dstPort).c_str());
		boost::asio::connect(socket_, endpoint);
		boost::asio::socket_base::keep_alive option(true);
		socket_.set_option(option);
	}
	catch (std::exception& error)
	{
		log_->write(std::string("[TCPClient doConnect Custom] ") + error.what(), Log::Level::ERROR);
	}
}

void TCPClient::doWrite(const HTTP::HttpType& httpType, 
  boost::asio::streambuf& buffer)
{
	try
	{
		moveStreamBuff(buffer, writeBuffer_);
		log_->write("[TCPClient doWrite] [DST " +
			socket_.remote_endpoint().address().to_string() +":"+
			std::to_string(socket_.remote_endpoint().port())+"] "+
			"[Bytes " +
			std::to_string(writeBuffer_.size())+"] ",
			Log::Level::DEBUG);
		boost::asio::write(
			socket_,
			writeBuffer_
		);
		switch (httpType){
			case HTTP::HttpType::https:
				if (config_->runMode() == RunMode::agent)
					doReadUntil("\r\nCOMP\r\n\r\n");
				else
					doReadSSL();
			break;
			case HTTP::HttpType::http:
				doRead();
			break;
			case HTTP::HttpType::connect:
				doReadUntil("\r\n\r\n");
			break;
		}
	}
	catch (std::exception& error)
	{
		log_->write(std::string("[TCPClient doWrite] ") + error.what(), Log::Level::ERROR);
	}
}

void TCPClient::doRead()
{
	try
	{
		readBuffer_.consume(readBuffer_.size());
		boost::system::error_code error;
		boost::asio::read(
			socket_,
			readBuffer_,
			error
		);
		if (!error || error == boost::asio::error::eof)
		{
			log_->write("[TCPClient doRead] [SRC " +
				socket_.remote_endpoint().address().to_string() +":"+
				std::to_string(socket_.remote_endpoint().port())+"] [Bytes " +
				std::to_string(readBuffer_.size())+"] ",
				Log::Level::DEBUG);
		} else
		{
			log_->write(std::string("[TCPClient doRead] ") + error.what(), Log::Level::ERROR);
		}
	}
	catch (std::exception& error)
	{
		log_->write(std::string("[TCPClient doRead] ") + error.what(), Log::Level::ERROR);
	}
}

void TCPClient::doReadUntil(const std::string& until)
{
	try
	{
		readBuffer_.consume(readBuffer_.size());
		boost::asio::read_until(
			socket_,
			readBuffer_,
			until
		);
		log_->write("[TCPClient doReadUntil] [SRC " +
			socket_.remote_endpoint().address().to_string() +":"+
			std::to_string(socket_.remote_endpoint().port())+"] [Bytes " +
			std::to_string(readBuffer_.size())+"] ",
			Log::Level::DEBUG);
	}
	catch (std::exception& error)
	{
		log_->write(std::string("[TCPClient doRead] ") + error.what(), Log::Level::ERROR);
	}
}

void TCPClient::doReadSSL()
{
	try
	{
		readBuffer_.consume(readBuffer_.size());
		boost::system::error_code error;
		boost::asio::streambuf tempBuff;
		unsigned short readExactly{1};
		bool end{false};
		bool isApplicationData{false};
		bool isApplicationDataVersion{false};
		bool isApplicationDataSize{false};

		while(!end){
			tempBuff.consume(tempBuff.size());
			boost::asio::read(
				socket_,
				tempBuff,
				boost::asio::transfer_exactly(readExactly),
				error
			);
			if (!error)
			{
				std::string tempBuffStr(
					hexArrToStr(
						reinterpret_cast<unsigned char*>(
							const_cast<char*>(
								streambufToString(tempBuff).c_str()
							)
						),
						tempBuff.size()
					)
				);
				if (tempBuffStr == "16")
				{
					copyStreamBuff(tempBuff, readBuffer_);
					boost::asio::streambuf newTempBuff;
					boost::asio::read(
						socket_,
						newTempBuff,
						boost::asio::transfer_exactly(2),
						error
					);
					copyStreamBuff(newTempBuff, readBuffer_);
					newTempBuff.consume(newTempBuff.size());
					boost::asio::read(
						socket_,
						newTempBuff,
						boost::asio::transfer_exactly(2),
						error
					);
					std::string newTempBuffStr(
						hexArrToStr(
							reinterpret_cast<unsigned char*>(
								const_cast<char*>(
									streambufToString(newTempBuff).c_str()
								)
							),
							newTempBuff.size()
						)
					);
					unsigned short newReadExactly {hexToInt(newTempBuffStr)};
					boost::asio::read(
						socket_,
						newTempBuff,
						boost::asio::transfer_exactly(newReadExactly),
						error
					);
					std::string finalTempBuffStr = hexArrToStr(
						reinterpret_cast<unsigned char*>(
							const_cast<char*>(
								streambufToString(newTempBuff).c_str()
							)
						),
						newTempBuff.size()
					);
					if (finalTempBuffStr == "00040e000000")
						end = true;
					copyStreamBuff(newTempBuff, readBuffer_);
					continue;
				}
				if (tempBuffStr == "17" && !isApplicationData)
				{
					isApplicationData = true;
					readExactly = 2;
					copyStreamBuff(tempBuff, readBuffer_);
					continue;
				}
				if (isApplicationData && !isApplicationDataVersion)
				{
					isApplicationDataVersion = true;
					readExactly = 2;
					copyStreamBuff(tempBuff, readBuffer_);
					continue;
				}
				if (isApplicationData && isApplicationDataVersion && !isApplicationDataSize)
				{
					isApplicationDataSize = true;
					readExactly = hexToInt(tempBuffStr);
					copyStreamBuff(tempBuff, readBuffer_);
					continue;
				}
				if (isApplicationData && isApplicationDataVersion && isApplicationDataSize)
					end = true;
				copyStreamBuff(tempBuff, readBuffer_);
			} else if (error == boost::asio::error::eof)
			{
				end = true;
			} else
			{
				end = true;
				log_->write(std::string("[TCPClient doRead] ") + error.what(), Log::Level::ERROR);
			}
		}
		log_->write("[TCPClient doReadSSL] [SRC " +
			socket_.remote_endpoint().address().to_string() +":"+
			std::to_string(socket_.remote_endpoint().port())+"] [Bytes " +
			std::to_string(readBuffer_.size())+"] ",
			Log::Level::DEBUG);
	}
	catch (std::exception& error)
	{
		log_->write(std::string("[TCPClient doReadSSL] ") + error.what(), Log::Level::ERROR);
	}
}
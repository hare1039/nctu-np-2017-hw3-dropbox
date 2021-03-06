#include <cstdlib>
#include <deque>
#include <iostream>
#include <list>
#include <memory>
#include <set>
#include <utility>
#include <map>
#include <sstream>

#include <boost/asio.hpp>

#include "filedata.hpp"

using boost::asio::ip::tcp;

typedef std::deque<filedata> filedata_queue;

class participant
{
public:
    virtual ~participant() {}
    virtual void deliver(const filedata& file) = 0;
};

typedef std::shared_ptr<participant> participant_ptr;

class room
{
	public:
	filedata_queue files_;
	std::set<participant_ptr> participants_;

	void join(participant_ptr p)
	{
		std::cout << "[room] join\n";
		participants_.insert(p);
		for(auto &file: this->files_)
			p->deliver(file);
	}

	void leave(participant_ptr p)
	{
		std::cout << "[room] leave\n";
		participants_.erase(p);
	}

	void deliver(const filedata& file, participant_ptr prohibited = nullptr)
	{
		std::cout << "[room] deliver\n";
		files_.push_back(file);
		std::cout << "# in room: " << participants_.size() << std::endl;
		for(auto participant: participants_)
			if(participant != prohibited)
				participant->deliver(file);
	}

	
};

class session : public participant,
                public std::enable_shared_from_this<session>
{
	tcp::socket    socket_;
	room&          room_;
	filedata       dataread_;
	filedata_queue filedatas_;
public:
	session(tcp::socket socket, room& room): socket_(std::move(socket)),
	                                         room_(room){}

	void start()
	{
		room_.join(shared_from_this());
		launch_async_read_header();
	}

	void deliver(const filedata& file)
	{
		auto sending = not filedatas_.empty();
		filedatas_.push_back(file);
		if(not sending)
			launch_async_write();
	}

private:
	void launch_async_read_header()
	{
		std::cout << "launch_async_read_header, supplied buffer size: " << dataread_.header_ref().size() << "\n";
		auto self(shared_from_this());
		boost::asio::async_read(
			socket_,
			boost::asio::buffer(dataread_.header_ref()),
			[this, self](boost::system::error_code err, std::size_t /*length*/)
			{
				if (not err && dataread_.try_decode_header_ok())
				{
					launch_async_read_body();
				}
				else
				{
					std::cout << "[session] lefted on reading header; reason: " << err.message() << std::endl;
					room_.leave(shared_from_this());
				}
			});
	}

	void launch_async_read_body()
	{
		std::cout << "launch_async_read_body, supplied buffer size: " << dataread_.body_ref().size() << "\n";
		auto self(shared_from_this());
		boost::asio::async_read(
			socket_,
			boost::asio::buffer(dataread_.body_ref()),
			[this, self](boost::system::error_code err, std::size_t /*length*/)
			{
				if (not err)
				{
					room_.deliver(dataread_, self);
					launch_async_read_header();
				}
				else
				{
					std::cout << "[session] lefted on reading body; reason: " << err.message() << std::endl;
					room_.leave(shared_from_this());
				}
			});
	}

	
	void launch_async_write()
	{
		std::cout << "launch_async_write\n";
		auto self(shared_from_this());
		boost::asio::async_write(
			socket_,
			boost::asio::buffer(filedatas_.front().data()),
			[this, self](boost::system::error_code err, std::size_t /*length*/){
				if(not err)
				{
					filedatas_.pop_front();
					if(not filedatas_.empty())
					{
						launch_async_write();
					}
				}
				else
				{
					std::cout << "[session] lefted on writing; reason: " << err.message() << std::endl;
					room_.leave(shared_from_this());
				}
			});
	}
};

class server
{
	tcp::acceptor acceptor_;
    tcp::socket   socket_;
	std::map<std::string, room> rooms_;
public:
	server(boost::asio::io_service& io,
	       const tcp::endpoint& endpoint): acceptor_(io, endpoint),
	                                       socket_(io) {
		std::cout << "launch accept\n";
		launch_accept();
	}

private:
	void launch_accept()
	{
		acceptor_.async_accept(
			socket_,
			[this](boost::system::error_code err)
			{
				if (not err)
				{
					// client will request for a room by name
					boost::asio::streambuf response; 
					auto size = boost::asio::read_until(socket_, response, "\n");
					auto bufs = response.data();
					std::string name(boost::asio::buffers_begin(bufs), boost::asio::buffers_begin(bufs) + size - 1);
					boost::system::error_code greet_err;
					boost::asio::write(socket_, boost::asio::buffer("Welcome to the dropbox-like server! " + name + "\n"), greet_err);
					if(not greet_err)
					{
						std::make_shared<session>(std::move(socket_), rooms_[name])->start();
						std::cout << "session started\nAll rooms are: [";
						for(auto &pair: rooms_)
							std::cout << "{" << pair.first << ", " << pair.second.participants_.size() << "}";
						std::cout << "]\n";
					}
					else
						std::cout << greet_err.message() << "\n";
				}

				launch_accept();
			});
	}
};


int main(int argc, char* argv[])
{
    try
    {
        if (argc < 2)
        {
            std::cerr << "Usage: server <port>\n";
            return 1;
        }

        boost::asio::io_service io_service;
        server serve(io_service, tcp::endpoint(tcp::v4(), std::stoi(argv[1])));
        io_service.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}

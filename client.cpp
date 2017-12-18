#include <cstdlib>
#include <deque>
#include <iostream>
#include <thread>
#include <boost/asio.hpp>
#include "filedata.hpp"

typedef std::deque<filedata> filedata_queue;
using boost::asio::ip::tcp;

class client
{
	std::string              name;
	boost::asio::io_service& io_service_;
    tcp::socket              socket_;
    filedata                 read_data_;
    filedata_queue           write_data_queue_;
public:
	client(std::string n, boost::asio::io_service& io, tcp::resolver::iterator endpoint_iterator):
		name(n),
		socket_(io),
		io_service_(io)
	{
		launch_connect(endpoint_iterator);
	}
	~client() {close();}

	void write(const filedata & file)
	{
		io_service_.post(
			[this, file]()
			{
				bool processing = not write_data_queue_.empty();
				write_data_queue_.push_back(file);
				if (not processing)
				{
					launch_async_write();
				}
			});
	}

	void close()
	{
		io_service_.post([this]{ socket_.close(); });
	}

	void sleep_for(int seconds)
	{
		io_service_.dispatch([seconds]{
			std::cout << "Client starts to sleep\n";
			using namespace std::chrono_literals;
			for(int count(1); count <= seconds; count++)
			{
				std::this_thread::sleep_for(1s);
				std::cout << "Sleep " << count << std::endl;
			}
			std::cout << "Client wakes up\n";
		});
	}
private:
	void launch_connect(tcp::resolver::iterator endpoint_iterator)
    {
        boost::asio::async_connect(
	        socket_,
	        endpoint_iterator,
	        [this](boost::system::error_code err, tcp::resolver::iterator)
	        {
		        if (not err)
		        {
			        boost::asio::write(socket_, boost::asio::buffer(this->name + "\n"));
			        boost::asio::streambuf response;
			        auto size = boost::asio::read_until(socket_, response, "\n");
                    auto bufs = response.data();
                    std::string buf(boost::asio::buffers_begin(bufs), boost::asio::buffers_begin(bufs) + size);
                    std::cout << buf;
			        launch_async_read_header();
		        }
		        else
			        std::cout << err.message() << std::endl;
	        });
    }
	void launch_async_read_header()
	{
		boost::asio::async_read(
			socket_,
			boost::asio::buffer(read_data_.header_ref()),
			[this](boost::system::error_code err, std::size_t /*length*/)
			{
				if (not err && read_data_.try_decode_header_ok())
				{
					launch_async_read_body();
				}
				else
				{
					std::cout << "[client] lefted on reading header; reason: " << err.message() << std::endl;
					socket_.close();
				}
			});
	}

	void launch_async_read_body()
	{
		std::cout << "Downloading file: " << read_data_.getfilename() << "\nProgress : [ ]" << std::endl;
		boost::asio::async_read(
			socket_,
			boost::asio::buffer(read_data_.body_ref()),
			[this](boost::system::error_code err, std::size_t /*length*/)
			{
				if (not err)
				{
					std::cout << "\33[2K\rProgress : [#]\nUploading file: " << read_data_.getfilename() << " complete.\n";
					read_data_.save();
					launch_async_read_header();
				}
				else
				{
					std::cout << "[client] lefted on reading body; reason: " << err.message() << std::endl;
					socket_.close();
				}
			});
	}

	void launch_async_write()
	{
		std::cout << "Uploading file: " << write_data_queue_.front().getfilename() << "\nProgress : [ ]";
		boost::asio::async_write(
			socket_,
			boost::asio::buffer(write_data_queue_.front().data()),
			[this](boost::system::error_code err, std::size_t /*length*/)
			{
				if(not err)
				{
					std::cout << "\33[2K\rProgress : [#]\nUploading file: " << write_data_queue_.front().getfilename() << " complete.\n";
					write_data_queue_.pop_front();
					if(not write_data_queue_.empty())
					{
						launch_async_write();
					}
				}
				else
				{
					std::cout << "[client] lefted on writing; reason: " << err.message() << std::endl;
					socket_.close();
				}
			});
	}
};


int main(int argc, char *argv[])
{
	try
    {
        if (argc != 4)
        {
            std::cerr << "Usage: client <host> <port> <name>\n";
            return 1;
        }
        boost::asio::io_service io_service;

        tcp::resolver resolver(io_service);
        auto endpoint_iterator = resolver.resolve({ argv[1], argv[2] });
        std::string line, name(argv[3]);
        client c(name, io_service, endpoint_iterator);

        std::thread t([&io_service](){ io_service.run(); });
        
        while (std::getline(std::cin, line))
        {
	        auto space = line.find(" ");
	        std::string command, argument;
	        if(space == std::string::npos)
	        {
		        command = line;
	        }
	        else
	        {
		        command  = line.substr(0, space);
		        argument = line.substr(space + 1);
	        }


	        if (command == "/put")
	        {
		        filedata file;
		        file.build_from(argument);
		        c.write(file);
	        }
	        else if (command == "/sleep")
	        {
		        c.sleep_for(std::stoi(argument));
	        }
	        else if (command == "/exit")
	        {
		        break;
	        }
	        else
	        {
		        //std::cout << "Unknown command: '" + command + "', argument: '" + argument + "'\n";
				std::system(line.c_str());
	        }
        }
        t.join();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}

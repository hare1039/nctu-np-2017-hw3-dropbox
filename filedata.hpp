#ifndef __FILEDATA_HPP__
#define __FILEDATA_HPP__

#include <vector>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <cstdint>

typedef std::vector<std::uint8_t> vbyte;

class filedata
{
	vbyte header_, body_, static_data_;
	std::string filename;
	static const std::size_t header_legnth = 40; // [0-3] length, [4-39] file name
public:
	filedata(): header_(header_legnth), body_(0){}

	std::size_t length() const {
		return header_.size() + body_.size();
	}

	vbyte& header_ref() {
        return header_;
	}
	vbyte& body_ref() {
		return body_;
	}

	vbyte& data() {
		header_.swap(static_data_);
		static_data_.insert(static_data_.end(), body_.begin(), body_.end());
		return static_data_;
	}

	void build_from(std::string filename)
	{
		std::fstream f(filename, std::ios::in | std::ios::binary);
		if(f.is_open())
		{
			f.unsetf(std::ios::skipws);
			{vbyte(header_legnth).swap(header_);}
			{vbyte().swap(body_);}
			body_.insert(body_.begin(),
			             std::istream_iterator<char>(f),
			             std::istream_iterator<char>());
			int size = static_cast<int>(body_.size());
			header_[3] = static_cast<std::uint8_t>(size >> 24);
			header_[2] = static_cast<std::uint8_t>(size >> 16);
            header_[1] = static_cast<std::uint8_t>(size >> 8);
			header_[0] = static_cast<std::uint8_t>(size);
			std::cout << "[filedata] encode:[3]:" << static_cast<int>(header_[3]) << " [2]:" << static_cast<int>(header_[2]) << " [1]:" << static_cast<int>(header_[1]) << " [0]:" << static_cast<int>(header_[0]) << "\n";
			int i(4);
			for(char c: filename)
			{
				header_.at(i++) = c;
				if(i == header_legnth)
					break;
			}
			this->filename = filename;
		}
	}

	void save()
	{
		std::ofstream f(filename, std::ios::out | std::ios::binary);
		if(f.is_open())
		{
			std::cout << "[filedata] " << this->filename << " saved\n";
			f.write(reinterpret_cast<char *>(body_.data()), body_.size());
		}
	}

	bool try_decode_header_ok()
	{
		int bodysize =
			header_[3] << 24 |
			header_[2] << 16 |
			header_[1] << 8  |
			header_[0];
		{vbyte(bodysize).swap(body_);}
		this->filename = "";
		for(int i(4); i < header_legnth; i++)
			this->filename += header_[i];
		std::cout << "[filedata] decode:{bodysize: " << bodysize << ", filename: " << this->filename << "}\n";
		return true;
	}
};

#endif//__FILEDATA_HPP__

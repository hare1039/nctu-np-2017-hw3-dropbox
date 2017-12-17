#ifndef __FILEDATA_HPP__
#define __FILEDATA_HPP__

#include <vector>

typedef std::vector<char> vbyte;

class filedata
{
	vbyte header_, body_;
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

	vbyte data() {
		vbyte cat(header_);
		cat.insert(cat.end(), body_.begin(), body_.end());
		return cat;
	}

	bool try_decode_header_ok()
	{
		int bodysize =
			header_[3] << 24 |
			header_[2] << 16 |
			header_[1] << 8  |
			header_[0];
		{vbyte(bodysize).swap(body_);}
		return true;
	}
};

#endif//__FILEDATA_HPP__

#pragma once
#include <iostream>

class membuf : public std::basic_streambuf<char>
{
private:
    const char* begin_;
    const char* end_;
    const char* current_;

public:
    membuf(const char* begin, const char* end) : begin_(begin), end_(end), current_(begin) 
    {
        setg((char*)begin_, (char*)current_, (char*)end_);
    }

    pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which = std::ios_base::in) override 
    {
        if (which != std::ios_base::in) 
            return pos_type(off_type(-1));

        char* newPos;
        switch (dir) 
        {
        case std::ios_base::beg:
            newPos = (char*)(begin_ + off);
            break;
        case std::ios_base::cur:
            newPos = (char*)(current_ + off);
            break;
        case std::ios_base::end:
            newPos = (char*)(end_ + off);
            break;
        default:
            return pos_type(off_type(-1));
        }

        if (newPos < begin_ || newPos > end_) 
            return pos_type(off_type(-1));

        current_ = newPos;
        setg((char*)begin_, (char*)current_, (char*)end_);
        return pos_type(current_ - begin_);
    }

    pos_type seekpos(pos_type pos, std::ios_base::openmode which = std::ios_base::in) override {
        return seekoff(pos - pos_type(off_type(0)), std::ios_base::beg, which);
    }
};

class memstream : public std::istream 
{
    membuf _buffer;
public:
    memstream(const char* begin, const char* end): std::istream(&_buffer), _buffer(begin, end)
    { }
};
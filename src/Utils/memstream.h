#pragma once
#include <iostream>
#include <cstdint>
#include <span>

class membuf : public std::basic_streambuf<char>
{
private:
    const char* begin_;
    const char* end_;

public:
    membuf(const char* begin, const char* end)
        : begin_(begin), end_(end)
    {
        setg((char*)begin_, (char*)begin_, (char*)end_);
    }

    pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which = std::ios_base::in) override
    {
        if (which != std::ios_base::in)
            return pos_type(off_type(-1));

        char* newPos = nullptr;
        switch (dir)
        {
        case std::ios_base::beg:
            newPos = (char*)(begin_ + off);
            break;
        case std::ios_base::cur:
            newPos = (char*)(gptr() + off);
            break;
        case std::ios_base::end:
            newPos = (char*)(end_ + off);
            break;
        default:
            return pos_type(off_type(-1));
        }

        if (newPos < begin_ || newPos > end_)
            return pos_type(off_type(-1));

        setg((char*)begin_, newPos, (char*)end_);
        return pos_type(newPos - begin_);
    }

    pos_type seekpos(pos_type pos, std::ios_base::openmode which = std::ios_base::in) override
    {
        return seekoff(pos - pos_type(off_type(0)), std::ios_base::beg, which);
    }
};

class memstream : public std::istream
{
    membuf _buffer;

public:
    memstream(std::span<const char> span)
        : std::istream(&_buffer), _buffer(span.data(), span.data() + span.size())
    {}

    memstream(std::span<const uint8_t> span)
        : std::istream(&_buffer), _buffer(reinterpret_cast<const char*>(span.data()), reinterpret_cast<const char*>(span.data() + span.size()))
    {}
};
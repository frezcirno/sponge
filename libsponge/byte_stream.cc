#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _cap{capacity} {}

size_t ByteStream::write(string &&data) {
    size_t data_size = data.size();

    if (!data_size)
        return 0;

    if (data_size + _size > _cap) {
        size_t left = _cap - _size;

        if (!left)
            return 0;

        _data.push_back(move(data.substr(0, left)));
        _size = _cap;
        _has_write += left;
        return left;
    }

    _data.push_back(move(data));
    _size += data_size;
    _has_write += data_size;
    return data_size;
}

size_t ByteStream::write(const string &data) {
    size_t data_size = data.size();

    if (!data_size)
        return 0;

    if (data_size + _size > _cap) {
        size_t left = _cap - _size;

        if (!left)
            return 0;

        _data.push_back(move(data.substr(0, left)));
        _size = _cap;
        _has_write += left;
        return left;
    }

    _data.push_back(data);
    _size += data_size;
    _has_write += data_size;
    return data_size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string res;
    size_t l = len;
    auto it = _data.begin();

    while (it != _data.end() && l >= it->size()) {
        l -= it->size();
        res.append(*it++);
    }

    if (it != _data.end() && l)
        res.append(it->begin(), it->begin() + l);

    return res;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t l = len;
    size_t fs;

    while (_data.size() && l >= (fs = _data.front().size())) {
        l -= fs;
        _size -= fs;
        _has_read += fs;
        _data.pop_front();
    }

    if (_data.size() && l) {
        _size -= l;
        _has_read += l;
        _data.front() = _data.front().substr(l);
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string res;
    res.reserve(min(len, buffer_size()));
    size_t l = len;

    while (_data.size() && l >= _data.front().size()) {
        l -= _data.front().size();
        res.append(_data.front());
        _data.pop_front();
    }

    if (_data.size() && l) {
        res.append(_data.front().begin(), _data.front().begin() + l);
        _data.front() = _data.front().substr(l);
    }

    _size -= res.size();
    _has_read += res.size();
    return res;
}

void ByteStream::end_input() { _end = true; }

bool ByteStream::input_ended() const { return _end; }

size_t ByteStream::buffer_size() const { return _size; }

bool ByteStream::buffer_empty() const { return !buffer_size(); }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _has_write; }

size_t ByteStream::bytes_read() const { return _has_read; }

size_t ByteStream::remaining_capacity() const { return _cap - buffer_size(); }

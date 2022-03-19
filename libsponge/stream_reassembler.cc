#include "stream_reassembler.hh"

#include <limits>

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _eof{numeric_limits<size_t>::max()} {}

bool StreamReassembler::cache_push(const uint64_t index, const std::string &data) {
    uint64_t wstart = unassembled(), wend = win_end();
    size_t begin = 0;
    if (wstart > index) {
        begin = wstart - index;
        if (begin >= data.size())
            return false;
    }

    size_t end = (wend > index ? wend - index : 0);
    if (end <= begin)
        return false;

    uint64_t i = begin + index;
    std::string_view d = std::string_view{data.c_str(), data.size()}.substr(begin, end - begin);

    size_t ie = i + d.size();
    for (auto it = _cache.begin(); it != _cache.end(); ++it) {
        auto k = it->first;
        auto v = it->second;
        size_t ke = k + v.size();
        if (k < ie && i < ke) {
            if (i < k)
                __cache_add(it, i, string(d.substr(0, k - i)));
            if (ie <= ke)
                return true;
            d.remove_prefix(ke - i);
            i = ke;
        }
    }

    __cache_add(_cache.end(), i, string(d));
    return true;
}

bool StreamReassembler::cache_push(const uint64_t index, std::string &&data) {
    uint64_t wstart = unassembled(), wend = win_end();
    size_t begin = 0;
    if (wstart > index) {
        begin = wstart - index;
        if (begin >= data.size())
            return false;
    }

    size_t end = (wend > index ? wend - index : 0);
    if (end <= begin)
        return false;

    uint64_t i = begin + index;
    StringBuffer d = StringBuffer{move(data)}.substr(begin, end - begin);

    size_t ie = i + d.size();
    for (auto it = _cache.begin(); it != _cache.end(); ++it) {
        auto k = it->first;
        auto v = it->second;
        size_t ke = k + v.size();
        if (k < ie && i < ke) {
            if (i < k)
                __cache_add(it, i, d.substr(0, k - i));
            if (ie <= ke)
                return true;
            d.remove_prefix(ke - i);
            i = ke;
        }
    }

    __cache_add(_cache.end(), i, d);
    return true;
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof)
        _eof = index + data.size();

    decltype(_cache)::iterator it;
    if (data.size() && cache_push(index, data)) {
        while (_cache.size() && (it = _cache.begin())->first <= unassembled()) {
            assemble(it->second);
            __cache_del(it);
        }
    }

    if (unassembled() == _eof)
        _output.end_input();
}

void StreamReassembler::push_substring(string &&data, const size_t index, const bool eof) {
    if (eof)
        _eof = index + data.size();

    decltype(_cache)::iterator it;
    if (data.size() && cache_push(index, move(data))) {
        while (_cache.size() && (it = _cache.begin())->first <= unassembled()) {
            assemble(it->second);
            __cache_del(it);
        }
    }

    if (unassembled() == _eof)
        _output.end_input();
}

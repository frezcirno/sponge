#include "stream_reassembler.hh"

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _eof{numeric_limits<size_t>::max()} {}

void StreamReassembler::__cache_add(const uint64_t index, const std::string &data) {
    _unass_bytes += data.size();
    _cache[index] = data;
}

void StreamReassembler::__cache_del(const uint64_t index) {
    _unass_bytes -= _cache[index].size();
    _cache.erase(index);
}

void StreamReassembler::__cache_append(const uint64_t index, const std::string &data) {
    _unass_bytes += data.size();
    _cache[index].append(data);
}

void StreamReassembler::cache_push(const uint64_t index, const std::string &data) {
    auto [i, d] = StreamReassembler::clamp(index, data, unassembled(), win_end());

    if (d.empty())
        return;

    size_t ie = i + d.size();
    for (auto [k, v] : _cache) {
        size_t ke = k + v.size();
        if (k < ie && i < ke) {
            if (i < k)
                __cache_add(i, d.substr(0, k - i));
            if (ie <= ke)
                return;
            d = d.substr(ke - i);
            i = ke;
        }
    }

    __cache_add(i, d);
}

std::pair<uint64_t, std::string> StreamReassembler::clamp(const size_t index,
                                                          const std::string &data,
                                                          uint64_t istart,
                                                          uint64_t iend) {
    size_t begin = (istart > index ? istart - index : 0);
    size_t end = (iend > index ? iend - index : 0);
    if (begin >= data.size())
        return {};
    return {index + begin, move(data.substr(begin, end - begin))};
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    uint64_t head;

    if (eof)
        _eof = index + data.size();

    cache_push(index, data);

    while (_cache.size() && (head = _cache.begin()->first) <= unassembled()) {
        assemble(_cache[head]);
        __cache_del(head);
    }

    if (unassembled() == _eof)
        _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const { return _unass_bytes; }

bool StreamReassembler::empty() const { return _cache.empty(); }

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

void StreamReassembler::__cache_merge() {
    if (_cache.size() < 2)
        return;

    for (std::map<uint64_t, std::string>::iterator p = _cache.begin(), i = std::next(p), n = std::next(i);
         i != _cache.end();
         i = n, n = std::next(i)) {
        auto &[lastk, lastv] = *p;
        auto [k, v] = *i;

        if (k <= lastk + lastv.size()) {
            if (lastk + lastv.size() < k + v.size())
                __cache_append(lastk, v.substr(lastk + lastv.size() - k));
            __cache_del(k);
            continue;
        }
        p = i;
    }
}

void StreamReassembler::__push_substring(const std::string &data, const size_t index) {
    if (_cache.count(index)) {
        if (data.size() <= _cache[index].size())
            return;

        __cache_append(index, data.substr(_cache[index].size()));
    } else
        __cache_add(index, data);

    __cache_merge();

    auto head = _cache.begin()->first;
    if (head <= unassembled()) {
        _output.write(_cache[head]);
        __cache_del(head);
        if (unassembled() == _eof)
            _output.end_input();
    }
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof)
        _eof = index + data.size();

    if (index < unassembled()) {
        if (index + data.size() > unassembled())
            __push_substring(data.substr(unassembled() - index), unassembled());
        return;
    }

    if (index + data.size() > win_end()) {
        if (index < win_end())
            __push_substring(data.substr(0, win_end() - index), index);
        return;
    }

    __push_substring(data, index);
}

size_t StreamReassembler::unassembled_bytes() const { return _unass_bytes; }

bool StreamReassembler::empty() const { return _cache.empty(); }

#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"
#include "string_buffer.hh"

#include <cstdint>
#include <map>
#include <string>

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.

    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes
    std::map<uint64_t, StringBuffer> _cache{};
    size_t _unass_bytes{}, _eof;
    size_t assemble(const StringBuffer &data) { return _output.write(data.copy()); }
    uint64_t unassembled() const { return _output.bytes_written(); }
    uint64_t win_end() const { return _output.bytes_read() + _capacity; }
    void __cache_add(const decltype(_cache)::const_iterator &it, const uint64_t index, const StringBuffer &data) {
        _unass_bytes += data.size();
        _cache.insert(it, {index, data});
    }
    void __cache_add(const decltype(_cache)::const_iterator &it, const uint64_t index, StringBuffer &&data) {
        _unass_bytes += data.size();
        _cache.insert(it, {index, std::move(data)});
    }
    void __cache_del(const decltype(_cache)::const_iterator &it) {
        _unass_bytes -= it->second.size();
        _cache.erase(it);
    }
    bool cache_push(const uint64_t index, const std::string &data);
    bool cache_push(const uint64_t index, std::string &&data);

  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);
    void push_substring(std::string &&data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const noexcept { return _output; }
    ByteStream &stream_out() noexcept { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const noexcept { return _unass_bytes; }

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const noexcept { return _cache.empty(); }
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

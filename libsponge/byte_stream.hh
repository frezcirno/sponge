#ifndef SPONGE_LIBSPONGE_BYTE_STREAM_HH
#define SPONGE_LIBSPONGE_BYTE_STREAM_HH

#include <deque>
#include "string_buffer.hh"

//! \brief An in-order byte stream.

//! Bytes are written on the "input" side and read from the "output"
//! side.  The byte stream is finite: the writer can end the input,
//! and then no more bytes can be written.
class ByteStream {
  private:
    // Your code here -- add private members as necessary.

    // Hint: This doesn't need to be a sophisticated data structure at
    // all, but if any of your tests are taking longer than a second,
    // that's a sign that you probably want to keep exploring
    // different approaches.

    bool _error{};  //!< Flag indicating that the stream suffered an error.
    std::deque<StringBuffer> _data{};
    size_t _size{}, _cap;
    size_t _has_read{}, _has_write{};
    bool _end{};

  public:
    //! Construct a stream with room for `capacity` bytes.
    ByteStream(const size_t capacity) : _cap{capacity} {}

    //! \name "Input" interface for the writer
    //!@{

    //! Write a string of bytes into the stream. Write as many
    //! as will fit, and return how many were written.
    //! \returns the number of bytes accepted into the stream
    size_t write(const std::string &data);
    size_t write(std::string &&data);
    size_t write(const StringBuffer &data);

    //! \returns the number of additional bytes that the stream has space for
    size_t remaining_capacity() const noexcept { return _cap - buffer_size(); }

    //! Signal that the byte stream has reached its ending
    void end_input() noexcept { _end = true; }

    //! Indicate that the stream suffered an error.
    void set_error() noexcept { _error = true; }
    //!@}

    //! \name "Output" interface for the reader
    //!@{

    //! Peek at next "len" bytes of the stream
    //! \returns a string
    std::string peek_output(const size_t len) const;

    //! Remove bytes from the buffer
    void pop_output(const size_t len);

    //! Read (i.e., copy and then pop) the next "len" bytes of the stream
    //! \returns a string
    std::string read(const size_t len);

    //! \returns `true` if the stream input has ended
    bool input_ended() const noexcept { return _end; }

    //! \returns `true` if the stream has suffered an error
    bool error() const noexcept { return _error; }

    //! \returns the maximum amount that can currently be read from the stream
    size_t buffer_size() const noexcept { return _size; }

    //! \returns `true` if the buffer is empty
    bool buffer_empty() const noexcept { return !buffer_size(); }

    //! \returns `true` if the output has reached the ending
    bool eof() const noexcept { return input_ended() && buffer_empty(); }
    //!@}

    //! \name General accounting
    //!@{

    //! Total number of bytes written
    size_t bytes_written() const noexcept { return _has_write; }

    //! Total number of bytes popped
    size_t bytes_read() const noexcept { return _has_read; }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_BYTE_STREAM_HH

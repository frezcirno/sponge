#ifndef SPONGE_LIBSPONGE_TCP_FACTORED_HH
#define SPONGE_LIBSPONGE_TCP_FACTORED_HH

#include "tcp_config.hh"
#include "tcp_receiver.hh"
#include "tcp_sender.hh"
#include "tcp_state.hh"

//! \brief A complete endpoint of a TCP connection
class TCPConnection {
  private:
    TCPConfig _cfg;
    TCPReceiver _receiver{_cfg.recv_capacity};
    TCPSender _sender{_cfg.send_capacity, _cfg.rt_timeout, _cfg.fixed_isn};

    //! outbound queue of segments that the TCPConnection wants sent
    std::queue<TCPSegment> _segments_out{};

    //! Should the TCPConnection stay active (and keep ACKing)
    //! for 10 * _cfg.rt_timeout milliseconds after both streams have ended,
    //! in case the remote TCPConnection doesn't know we've received its whole stream?
    bool _linger_after_streams_finish{true};

    size_t _time_since_last_segment_received{};

    bool _error{false};

    bool _active{true};

  public:
    ByteStream &outbound_stream() { return _sender.stream_in(); }

    const ByteStream &outbound_stream() const { return _sender.stream_in(); }

    const ByteStream &inbound_stream() const { return _receiver.stream_out(); }

  private:
    void _sender_flush() {
        auto &seg_out = _sender.segments_out();
        while (seg_out.size()) {
            auto &seg = seg_out.front();
            __set_ack(seg);
            segments_out().push(std::move(seg));
            seg_out.pop();
        }
    }

    void __set_ack(TCPSegment &seg) const {
        auto ackno = _receiver.ackno();
        if (ackno.has_value()) {
            seg.header().ack = true;
            seg.header().ackno = ackno.value();
            seg.header().win = std::min(static_cast<size_t>(UINT16_MAX), _receiver.window_size());
        }
    }

    void _set_error() {
        outbound_stream().set_error();
        inbound_stream().set_error();
        _linger_after_streams_finish = false;
        _error = true;
        _active = false;
    }

    void _reset() {
        _sender.segments_out() = {};

        _sender.send_empty_segment();
        _sender.segments_out().front().header().rst = true;
        segments_out().push(_sender.segments_out().front());
        _sender.segments_out().pop();
    }

  public:
    bool _closed() const { return !_receiver.syn_rcvd() && !_sender.syn_sent(); }

    bool _listen() const { return _closed(); }

    bool _syn_sent() const { return _sender.syn_sent() && !_receiver.syn_rcvd() && !_sender.syn_acked(); }

    bool _syn_rcvd() const { return _sender.syn_sent() && _receiver.syn_rcvd() && !_sender.syn_acked(); }

    bool _established() const { return _sender.syn_sent() && _receiver.syn_rcvd() && _sender.syn_acked(); }

    bool _fin_wait_1() const { return _sender.fin_sent() && !_receiver.fin_rcvd() && !_sender.fin_acked(); }

    bool _closing() const { return _sender.fin_sent() && _receiver.fin_rcvd() && !_sender.fin_acked(); }

    bool _fin_wait_2() const { return _sender.fin_sent() && !_receiver.fin_rcvd() && _sender.fin_acked(); }

    bool _time_wait() const { return _sender.fin_sent() && _receiver.fin_rcvd() && _sender.fin_acked(); }

    bool _close_wait() const { return !_sender.fin_sent() && _receiver.fin_rcvd() && !_sender.fin_acked(); }

    bool _last_ack() const { return _sender.fin_sent() && _receiver.fin_rcvd() && !_sender.fin_acked(); }

    const TCPState _state() const {
        if (_closed())
            return TCPState::State::CLOSED;
        if (_listen())
            return TCPState::State::LISTEN;
        if (_syn_sent())
            return TCPState::State::SYN_SENT;
        if (_syn_rcvd())
            return TCPState::State::SYN_RCVD;
        if (_established())
            return TCPState::State::ESTABLISHED;
        if (_fin_wait_1())
            return TCPState::State::FIN_WAIT_1;
        if (_closing())
            return TCPState::State::CLOSE_WAIT;
        if (_fin_wait_2())
            return TCPState::State::FIN_WAIT_2;
        if (_time_wait())
            return TCPState::State::TIME_WAIT;
        if (_close_wait())
            return TCPState::State::CLOSE_WAIT;
        if (_last_ack())
            return TCPState::State::LAST_ACK;
        return TCPState::State::RESET;
    }

  public:
    //! \name "Input" interface for the writer
    //!@{

    //! \brief Initiate a connection by sending a SYN segment
    void connect() {
        _sender.fill_window();
        _sender_flush();
    }

    //! \brief Write data to the outbound byte stream, and send it over TCP if possible
    //! \returns the number of bytes from `data` that were actually written.
    size_t write(const std::string &data);

    //! \returns the number of `bytes` that can be written right now.
    size_t remaining_outbound_capacity() const { return outbound_stream().remaining_capacity(); }

    //! \brief Shut down the outbound byte stream (still allows reading incoming data)
    void end_input_stream() {
        if (!active())
            return;
        outbound_stream().end_input();
        _sender.fill_window();
        _sender_flush();
    }
    //!@}

    //! \name "Output" interface for the reader
    //!@{

    //! \brief The inbound byte stream received from the peer
    ByteStream &inbound_stream() { return _receiver.stream_out(); }
    //!@}

    //! \name Accessors used for testing

    //!@{
    //! \brief number of bytes sent and not yet acknowledged, counting SYN/FIN each as one byte
    size_t bytes_in_flight() const { return _sender.bytes_in_flight(); }
    //! \brief number of bytes not yet reassembled
    size_t unassembled_bytes() const { return _receiver.unassembled_bytes(); }
    //! \brief Number of milliseconds since the last segment was received
    size_t time_since_last_segment_received() const { return _time_since_last_segment_received; }
    //!< \brief summarize the state of the sender, receiver, and the connection
    TCPState state() const { return {_sender, _receiver, active(), _linger_after_streams_finish}; };
    //!@}

    //! \name Methods for the owner or operating system to call
    //!@{

    //! Called when a new segment has been received from the network
    void segment_received(const TCPSegment &seg);

    //! Called periodically when time elapses
    void tick(const size_t ms_since_last_tick);

    //! \brief TCPSegments that the TCPConnection has enqueued for transmission.
    //! \note The owner or operating system will dequeue these and
    //! put each one into the payload of a lower-layer datagram (usually Internet datagrams (IP),
    //! but could also be user datagrams (UDP) or any other kind).
    std::queue<TCPSegment> &segments_out() { return _segments_out; }

    //! \brief Is the connection still alive in any way?
    //! \returns `true` if either stream is still running or if the TCPConnection is lingering
    //! after both streams have finished (e.g. to ACK retransmissions from the peer)
    bool active() const;
    //!@}

    //! Construct a new connection from a configuration
    explicit TCPConnection(const TCPConfig &cfg) : _cfg{cfg} {}

    //! \name construction and destruction
    //! moving is allowed; copying is disallowed; default construction not possible

    //!@{
    ~TCPConnection();  //!< destructor sends a RST if the connection is still open
    TCPConnection() = delete;
    TCPConnection(TCPConnection &&other) = default;
    TCPConnection &operator=(TCPConnection &&other) = default;
    TCPConnection(const TCPConnection &other) = delete;
    TCPConnection &operator=(const TCPConnection &other) = delete;
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_FACTORED_HH

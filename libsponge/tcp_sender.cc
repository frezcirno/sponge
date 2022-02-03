#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _rto{retx_timeout} {}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _recv_ackno; }

void TCPSender::fill_window() {
    uint16_t recv_win = _recv_win ? _recv_win : 1;

    while (_next_seqno < _recv_ackno + recv_win) {
        TCPSegment seg;
        size_t left_win = _recv_ackno + recv_win - _next_seqno;

        seg.header().seqno = wrap(_next_seqno, _isn);

        if (!_synd) {
            seg.header().syn = true;
            _synd = true;
        }

        if (seg.length_in_sequence_space() < left_win) {
            size_t readn = min(TCPConfig::MAX_PAYLOAD_SIZE, left_win - seg.length_in_sequence_space());
            seg.payload() = Buffer(move(_stream.read(readn)));
        }

        if (seg.length_in_sequence_space() < left_win) {
            if (!_find && _stream.eof()) {
                seg.header().fin = true;
                _find = true;
            }
        }

        if (!seg.length_in_sequence_space())
            break;

        if (!_rto_timer)
            _rto_timer = _rto;
        _next_seqno += seg.length_in_sequence_space();
        _segments_out.push(seg);
        _write_queue.push_back(seg);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // 0----------------------------------------->
    // ----------[rack..........r+w)------------->
    // ------------------[..............)-------->
    uint64_t ackno_unw = unwrap(ackno, _isn, _recv_ackno);

    if (ackno_unw < _recv_ackno || _next_seqno < ackno_unw) {
        return;
    }

    _recv_ackno = ackno_unw;
    _recv_win = window_size;

    bool write_out = false;
    for (auto i = _write_queue.cbegin(), n = next(i); i != _write_queue.end(); i = n, n = next(i)) {
        uint64_t seqno = unwrap(i->header().seqno, _isn, _recv_ackno);
        if (seqno + i->length_in_sequence_space() <= _recv_ackno) {
            _write_queue.pop_front();
            write_out = true;
        } else
            break;
    }

    if (write_out) {
        _retrans_cnt = 0;
        _rto = _initial_retransmission_timeout;
        _rto_timer = (_write_queue.empty() ? 0 : _rto);
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (_rto_timer) {
        _rto_timer -= ms_since_last_tick;
        if (_rto_timer <= 0) {
            _segments_out.push(_write_queue.front());

            if (_recv_win) {
                _retrans_cnt++;
                _rto *= 2;
            }

            _rto_timer = _rto;
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _retrans_cnt; }

void TCPSender::send_empty_segment() { _segments_out.push({}); }

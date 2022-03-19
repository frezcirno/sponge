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
    , _timer{retx_timeout} {}

void TCPSender::fill_window() {
    uint16_t recv_win = _recv_win ? _recv_win : 1;

    while (_next_seqno < _recv_ackno + recv_win) {
        TCPSegment seg;
        size_t seg_size = 0;
        size_t left_win = _recv_ackno + recv_win - _next_seqno;

        if (!syn_sent()) {
            seg.header().syn = true;
            seg_size++;
        }

        if (size_t remain = left_win - seg_size; remain > 0) {
            size_t readn = min(TCPConfig::MAX_PAYLOAD_SIZE, remain);
            seg.payload() = stream_in().read(readn);
            seg_size += seg.payload().size();
        }

        if (seg_size < left_win && !fin_sent() && stream_in().eof()) {
            seg.header().fin = true;
            seg_size++;
        }

        if (!seg_size)
            break;

        seg.header().seqno = wrap(_next_seqno, _isn);
        _next_seqno += seg_size;
        segments_out().push(seg);
        _write_queue.push_back(move(seg));
        if (_timer.stopped())
            _timer.start();
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t ackno_abs = unwrap(ackno, _isn, next_seqno_absolute());

    if (ackno_abs < _recv_ackno || next_seqno_absolute() < ackno_abs)
        return;

    _recv_ackno = ackno_abs;
    _recv_win = window_size;

    bool write_out = false;
    for (auto it = _write_queue.begin(); it != _write_queue.end();) {
        uint64_t seqno_abs = unwrap(it->header().seqno, _isn, next_seqno_absolute());
        if (seqno_abs + it->length_in_sequence_space() <= _recv_ackno) {
            it = _write_queue.erase(it);
            write_out = true;
        } else {
            break;
        }
    }

    if (write_out) {
        _retrans_cnt = 0;
        _timer.setup(_initial_retransmission_timeout);
        if (!_write_queue.empty())
            _timer.start();
        else
            _timer.reset();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (_timer.tick(ms_since_last_tick)) {
        segments_out().push(_write_queue.front());

        if (_recv_win) {
            _retrans_cnt++;
            _timer.setup(2 * _timer.time());
        }

        _timer.start();
    }
}

void TCPSender::send_empty_segment() {
    segments_out().push({});
    segments_out().back().header().seqno = wrap(next_seqno_absolute(), _isn);
}

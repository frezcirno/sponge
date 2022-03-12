#include "tcp_connection.hh"

#include <iostream>

using namespace std;

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (!active())
        return;

    if (!_sender.syn_sent() && seg.header().ack)
        return;

    if (seg.header().rst) {
        _set_error();
        return;
    }

    _time_since_last_segment_received = 0;

    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _sender.fill_window();
        _sender_flush();
    }

    _receiver.segment_received(seg);

    if (seg.length_in_sequence_space()) {
        _sender.fill_window();
        if (_sender.segments_out().empty())
            _sender.send_empty_segment();
        _sender_flush();
    } else if (_receiver.syn_rcvd() && seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
        _sender_flush();
    }

    if (_receiver.fin_rcvd() && !outbound_stream().eof())
        _linger_after_streams_finish = false;
}

//! \brief Is the connection still alive in any way?
//! \returns `true` if either stream is still running or if the TCPConnection is lingering
//! after both streams have finished (e.g. to ACK retransmissions from the peer)
bool TCPConnection::active() const {
    if (_error)
        return false;

    if (!_receiver.fin_rcvd() || !outbound_stream().eof() || _sender.bytes_in_flight())
        return true;

    if (!_linger_after_streams_finish || time_since_last_segment_received() >= 10 * _cfg.rt_timeout)
        return false;

    return true;
}

size_t TCPConnection::write(const string &data) {
    size_t res = outbound_stream().write(data);
    _sender.fill_window();
    _sender_flush();
    return res;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!active())
        return;

    _time_since_last_segment_received += ms_since_last_tick;

    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        _set_error();
        _reset();
        return;
    }

    _sender_flush();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            _reset();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

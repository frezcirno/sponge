#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    auto &header = seg.header();

    if (_synd && header.syn)
        return;

    if (header.syn) {
        _isn = header.seqno;
        _synd = true;
    }

    if (_synd) {
        auto &payload = seg.payload();
        uint64_t payload_idx = to_index(header.seqno, header.syn);

        if (payload.size()) {
            _reassembler.push_substring(payload.copy(), payload_idx, false);
        }

        if (header.fin) {
            _reassembler.push_substring({}, payload_idx + payload.size(), true);
        }
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_synd)
        return {};
    uint32_t ack = _reassembler.stream_out().bytes_written();
    if (_synd)
        ack++;
    if (_reassembler.stream_out().input_ended())
        ack++;
    return wrap(ack, _isn);
}

size_t TCPReceiver::window_size() const { return _reassembler.stream_out().remaining_capacity(); }

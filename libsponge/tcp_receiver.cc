#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const auto &header = seg.header();

    if (!(header.syn ^ syn_rcvd()))
        return;

    if (header.syn) {
        _isn = header.seqno;
        _reassembler.push_substring(seg.payload().copy(), 0, header.fin);
    } else { /* syn_rcvd() */
        _reassembler.push_substring(
            seg.payload().copy(), unwrap(header.seqno, _isn.value(), ackno_absolute()) - 1, header.fin);
    }
}

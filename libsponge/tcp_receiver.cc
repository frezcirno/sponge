#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    auto &&header = seg.header();

    if (!(syn_rcvd() ^ header.syn))
        return;

    if (header.syn)
        _isn = header.seqno;

    if (syn_rcvd()) {
        uint64_t index = unwrap(header.seqno, _isn.value(), ackno_absolute());

        _reassembler.push_substring(seg.payload().copy(), header.syn ? 0 : index - 1, header.fin);
    }
}

#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    EthernetFrame frame;
    frame.header().type = EthernetHeader::TYPE_IPv4;
    frame.header().src = _ethernet_address;
    frame.payload() = dgram.serialize();

    if (_arp_tbl.count(next_hop_ip) && _arp_tbl[next_hop_ip].addr.has_value()) {
        frame.header().dst = _arp_tbl[next_hop_ip].addr.value();
        frames_out().push(move(frame));
    } else {
        arp_request(next_hop_ip);
        _pending_frame[next_hop_ip].push_back(move(frame));
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        if (frame.header().dst == _ethernet_address || frame.header().dst == ETHERNET_BROADCAST) {
            auto dg = InternetDatagram();
            if (dg.parse(frame.payload()) == ParseResult::NoError)
                return dg;
        }
    } else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        auto msg = ARPMessage();
        if (msg.parse(frame.payload()) == ParseResult::NoError) {
            learn(msg.sender_ip_address, msg.sender_ethernet_address);

            if (msg.opcode == ARPMessage::OPCODE_REQUEST) {
                if (msg.target_ip_address == _ip_address.ipv4_numeric())
                    arp_reply(msg.sender_ethernet_address, msg.sender_ip_address);
            } else if (msg.opcode == ARPMessage::OPCODE_REPLY) {
            }
        }
    }

    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _tick += ms_since_last_tick;

    for (auto it = _arp_tbl.begin(); it != _arp_tbl.end();) {
        const auto &entry = it->second;
        if ((!entry.addr.has_value() && _tick - entry.request_ts > 5 * 1000) ||
            (entry.update_ts && _tick - entry.update_ts > 30 * 1000)) {
            it = _arp_tbl.erase(it);
        } else {
            ++it;
        }
    }
}

void NetworkInterface::arp_send(const EthernetAddress &target, uint16_t opcode, uint32_t target_ip) {
    EthernetFrame frame;
    frame.header().src = _ethernet_address;
    frame.header().dst = target;
    frame.header().type = EthernetHeader::TYPE_ARP;

    ARPMessage msg;
    msg.opcode = opcode;
    msg.sender_ethernet_address = _ethernet_address;
    msg.sender_ip_address = _ip_address.ipv4_numeric();
    msg.target_ethernet_address = (target != ETHERNET_BROADCAST ? target : EthernetAddress{});
    msg.target_ip_address = target_ip;
    frame.payload() = msg.serialize();

    frames_out().push(move(frame));
}

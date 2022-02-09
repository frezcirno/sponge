#ifndef SPONGE_LIBSPONGE_NETWORK_INTERFACE_HH
#define SPONGE_LIBSPONGE_NETWORK_INTERFACE_HH

#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "tcp_over_ip.hh"
#include "tun.hh"

#include <list>
#include <optional>
#include <queue>
#include <unordered_map>

//! \brief A "network interface" that connects IP (the internet layer, or network layer)
//! with Ethernet (the network access layer, or link layer).

//! This module is the lowest layer of a TCP/IP stack
//! (connecting IP with the lower-layer network protocol,
//! e.g. Ethernet). But the same module is also used repeatedly
//! as part of a router: a router generally has many network
//! interfaces, and the router's job is to route Internet datagrams
//! between the different interfaces.

//! The network interface translates datagrams (coming from the
//! "customer," e.g. a TCP/IP stack or router) into Ethernet
//! frames. To fill in the Ethernet destination address, it looks up
//! the Ethernet address of the next IP hop of each datagram, making
//! requests with the [Address Resolution Protocol](\ref rfc::rfc826).
//! In the opposite direction, the network interface accepts Ethernet
//! frames, checks if they are intended for it, and if so, processes
//! the the payload depending on its type. If it's an IPv4 datagram,
//! the network interface passes it up the stack. If it's an ARP
//! request or reply, the network interface processes the frame
//! and learns or replies as necessary.
class NetworkInterface {
  private:
    //! Ethernet (known as hardware, network-access-layer, or link-layer) address of the interface
    EthernetAddress _ethernet_address;

    //! IP (known as internet-layer or network-layer) address of the interface
    Address _ip_address;

    //! outbound queue of Ethernet frames that the NetworkInterface wants sent
    std::queue<EthernetFrame> _frames_out{};

    struct ARPEntry {
        std::optional<EthernetAddress> addr{};
        size_t update_ts{};
        size_t request_ts{};
    };
    std::unordered_map<uint32_t, ARPEntry> _arp_tbl{};
    std::unordered_map<uint32_t, std::list<EthernetFrame>> _pending_frame{};
    size_t _tick{42};

    void learn(uint32_t ip, EthernetAddress eth) {
        _arp_tbl[ip].addr = eth;
        _arp_tbl[ip].update_ts = _tick;

        for (auto &&frame : _pending_frame[ip]) {
            frame.header().dst = eth;
            frames_out().push(frame);
        }

        _pending_frame.erase(ip);
    }

    void arp_send(const EthernetAddress &target, uint16_t opcode, uint32_t target_ip);

    void arp_request(uint32_t ip_addr) {
        if (_arp_tbl.count(ip_addr) && !_arp_tbl[ip_addr].addr.has_value())
            return;

        arp_send(ETHERNET_BROADCAST, ARPMessage::OPCODE_REQUEST, ip_addr);
        _arp_tbl[ip_addr].request_ts = _tick;
    }

    void arp_reply(const EthernetAddress &target, uint32_t target_ip) {
        arp_send(target, ARPMessage::OPCODE_REPLY, target_ip);
    }

  public:
    //! \brief Construct a network interface with given Ethernet (network-access-layer) and IP (internet-layer) addresses
    NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address);

    //! \brief Access queue of Ethernet frames awaiting transmission
    std::queue<EthernetFrame> &frames_out() { return _frames_out; }

    //! \brief Sends an IPv4 datagram, encapsulated in an Ethernet frame (if it knows the Ethernet destination address).

    //! Will need to use [ARP](\ref rfc::rfc826) to look up the Ethernet destination address for the next hop
    //! ("Sending" is accomplished by pushing the frame onto the frames_out queue.)
    void send_datagram(const InternetDatagram &dgram, const Address &next_hop);

    //! \brief Receives an Ethernet frame and responds appropriately.

    //! If type is IPv4, returns the datagram.
    //! If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
    //! If type is ARP reply, learn a mapping from the "sender" fields.
    std::optional<InternetDatagram> recv_frame(const EthernetFrame &frame);

    //! \brief Called periodically when time elapses
    void tick(const size_t ms_since_last_tick);
};

#endif  // SPONGE_LIBSPONGE_NETWORK_INTERFACE_HH

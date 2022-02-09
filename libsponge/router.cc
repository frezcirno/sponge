#include "router.hh"

#include <iostream>

using namespace std;

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    _routes.push_back({route_prefix,
                       prefix_length ? static_cast<uint32_t>(INT32_MIN >> (prefix_length - 1)) : 0,
                       next_hop,
                       interface_num});
    std::sort(
        _routes.begin(), _routes.end(), [](const auto &r1, const auto &r2) { return r1.prefix_mask > r2.prefix_mask; });
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    if (dgram.header().ttl <= 1)
        return;

    for (auto &&r : _routes) {
        if (match(r.route_prefix, dgram.header().dst, r.prefix_mask)) {
            dgram.header().ttl--;
            interface(r.interface_num)
                .send_datagram(dgram, r.next_hop.value_or(Address::from_ipv4_numeric(dgram.header().dst)));
            break;
        }
    }
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}

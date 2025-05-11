#include "router.hh"

#include <cassert>
#include <optional>

#include "ipv4_datagram.hh"

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  assert( prefix_length <= 32 );
  assert( interface_num < interfaces_.size() );
  // If the prefix exists, overwrite the old `RouteDestination`.
  const uint32_t prefix { Prefix( route_prefix, prefix_length ) };
  const RouteDestination route_destination { next_hop, interface_num };
  routing_table_[prefix_length].insert( { prefix, route_destination } );
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  for ( auto interface : interfaces_ ) {
    std::queue<InternetDatagram>& datagrams { interface->datagrams_received() };
    while ( !datagrams.empty() ) {
      InternetDatagram datagram = datagrams.front();
      datagrams.pop();
      Forward( datagram );
    }
  }
}

void Router::Forward( InternetDatagram& datagram )
{
  if ( datagram.header.ttl <= 1 ) {
    return;
  }
  --datagram.header.ttl;
  datagram.header.compute_checksum();

  // Longest prefix match
  const std::optional<RouteDestination> route_destination { [&] {
    for ( int8_t prefix_length = 32; 0 <= prefix_length; --prefix_length ) {
      const auto& route_destinations { routing_table_[prefix_length] };
      const uint32_t prefix = Prefix( datagram.header.dst, prefix_length );
      const auto search { route_destinations.find( prefix ) };
      if ( search != route_destinations.cend() ) {
        return std::make_optional( search->second );
      }
    }
    return std::optional<RouteDestination> {};
  }() };
  if ( !route_destination ) {
    return;
  }

  const auto target_interface { interface( route_destination->interface_num ) };
  const Address next_hop_address { route_destination->next_hop
                                     ? *route_destination->next_hop
                                     : Address::from_ipv4_numeric( datagram.header.dst ) };
  target_interface->send_datagram( datagram, next_hop_address );
}

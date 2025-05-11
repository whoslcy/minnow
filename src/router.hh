#pragma once

#include <array>
#include <map>
#include <optional>

#include "exception.hh"
#include "network_interface.hh"

// \brief A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.
class Router
{
public:
  // Add an interface to the router
  // \param[in] interface an already-constructed network interface
  // \returns The index of the interface after it has been added to the router
  size_t add_interface( std::shared_ptr<NetworkInterface> interface )
  {
    interfaces_.push_back( notnull( "add_interface", std::move( interface ) ) );
    return interfaces_.size() - 1;
  }

  // Access an interface by index
  std::shared_ptr<NetworkInterface> interface( const size_t N ) { return interfaces_.at( N ); }

  // Add a route (a forwarding rule)
  void add_route( uint32_t route_prefix,
                  uint8_t prefix_length,
                  std::optional<Address> next_hop,
                  size_t interface_num );

  // Route packets between the interfaces
  void route();

private:
  struct RouteDestination
  {
    std::optional<Address> next_hop;
    size_t interface_num;
  };

  uint32_t Prefix( uint32_t ipv4_numeric, uint8_t prefix_length ) const
  {
    return prefix_length ? ipv4_numeric >> ( 32 - prefix_length ) : 0;
  }

  void Forward( InternetDatagram& datagram );

  // Prefix lengths: [0, 32]
  static constexpr size_t kCountOfPrefixLength = 33;

  std::array<std::map<uint32_t, RouteDestination>, kCountOfPrefixLength> routing_table_ {};

  // The router's collection of network interfaces
  std::vector<std::shared_ptr<NetworkInterface>> interfaces_ {};
};

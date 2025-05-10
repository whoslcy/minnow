#include "wrapping_integers.hh"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <optional>

#include "debug.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  return Wrap32 { static_cast<uint32_t>( n + zero_point.raw_value_ ) };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  auto UnwrapCandidate = [&]( const uint32_t period_index ) -> uint64_t {
    constexpr uint64_t kPeriodLength { 1ul << 32 };
    const uint64_t ISN { zero_point.raw_value_ }; // Initial Sequence Number.
    const uint64_t unwrap_base { raw_value_ < ISN ? kPeriodLength - ISN + raw_value_ : raw_value_ - ISN };
    return unwrap_base + period_index * kPeriodLength;
  };
  const uint64_t period_index { checkpoint >> 32 };
  const std::array<uint64_t, 3> candidates {
    UnwrapCandidate( ( period_index == 0 ? period_index : period_index - 1 ) ),
    UnwrapCandidate( period_index ),
    UnwrapCandidate( period_index + 1 ) };
  return *std::ranges::min_element( candidates, [&]( const uint64_t lhs, const uint64_t rhs ) {
    return Unsigned64Distance( lhs, checkpoint ) < Unsigned64Distance( rhs, checkpoint );
  } );
}

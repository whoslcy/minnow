#include "reassembler.hh"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <utility>

#include "debug.hh"

using namespace std;

void Reassembler::insert( ByteStreamIndex first_index, string data, bool is_last_substring )
{
  // The valid range of `data`.
  const ByteStreamIndex valid_first { std::max( first_index, FirstUnassembled() ) };
  const ByteStreamIndex valid_after_final { std::min( first_index + data.size(), FirstUnaccepted() ) };

  // Update the pending bytes with the valid range of `data`.
  for ( ByteStreamIndex i { valid_first }; i < valid_after_final; ++i ) {
    PendingAt( i ) = data[i - first_index];
  }

  // Try pushing bytes to the writer.
  const std::string pushable { [&] {
    std::string s;
    for ( ByteStreamIndex i { FirstUnassembled() }; i < FirstUnaccepted() && PendingAt( i ).has_value(); ++i ) {
      s += *PendingAt( i );
    }
    return s;
  }() };
  GetWriter().push( pushable );
  pending_.erase( pending_.begin(), pending_.begin() + pushable.size() );
  pending_.resize( capacity_, std::nullopt );

  if ( is_last_substring ) {
    index_of_after_final_byte_ = first_index + data.size();
  }
  if ( index_of_after_final_byte_.has_value() ) {
    const bool final_byte_is_pushed { FirstUnassembled() == *index_of_after_final_byte_ };
    if ( final_byte_is_pushed ) {
      GetWriter().close();
    }
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t count { 0 };
  for ( ByteStreamIndex i { FirstUnassembled() }; i < FirstUnaccepted(); ++i ) {
    if ( PendingAt( i ).has_value() ) {
      ++count;
    }
  }
  return count;
}

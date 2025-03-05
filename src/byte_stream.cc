#include "byte_stream.hh"

#include <cassert>
#include <limits>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  if ( is_closed() ) {
    return;
  }
  TestError();
  if ( has_error() ) {
    return;
  }

  uint64_t count_of_bytes_to_push = std::min( data.size(), available_capacity() );
  buffer_.append( data.data(), count_of_bytes_to_push );

  constexpr uint64_t uint64_max = std::numeric_limits<uint64_t>::max();
  if ( pushed_bytes_count_ < uint64_max - count_of_bytes_to_push ) {
    pushed_bytes_count_ += count_of_bytes_to_push;
    return;
  }
  pushed_bytes_count_ = uint64_max;
  set_error();
}

void Writer::close()
{
  refuse_new_data_ = true;
}

bool Writer::is_closed() const
{
  return refuse_new_data_;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - buffer_.size();
}

uint64_t Writer::bytes_pushed() const
{
  return pushed_bytes_count_;
}

string_view Reader::peek() const
{
  return buffer_;
}

void Reader::pop( uint64_t len )
{
  if ( is_finished() ) {
    return;
  }
  TestError();
  if ( has_error() ) {
    return;
  }

  uint64_t count_of_bytes_to_pop = std::min( len, buffer_.size() );
  buffer_.erase( 0, count_of_bytes_to_pop );
  constexpr uint64_t uint64_max = std::numeric_limits<uint64_t>::max();
  if ( popped_bytes_count_ < uint64_max - count_of_bytes_to_pop ) {
    popped_bytes_count_ += count_of_bytes_to_pop;
    return;
  }
  pushed_bytes_count_ = uint64_max;
  set_error();
}

bool Reader::is_finished() const
{
  return refuse_new_data_ && buffer_.empty();
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_.size();
}

uint64_t Reader::bytes_popped() const
{
  return popped_bytes_count_;
}

#pragma once

#include <cstddef>
#include <optional>
#include <queue>
#include <vector>

#include "byte_stream.hh"

class Reassembler
{
  using ByteStreamIndex = uint64_t;

public:
  // Construct Reassembler to write into given ByteStream.
  explicit Reassembler( ByteStream&& output )
    : output_( std::move( output ) )
    , capacity_ { GetWriter().available_capacity() }
    , pending_( capacity_, std::nullopt )
  {}

  /*
   * Insert a new substring to be reassembled into a ByteStream.
   *   `first_index`: the index of the first byte of the substring
   *   `data`: the substring itself
   *   `is_last_substring`: this substring represents the end of the stream
   *   `output`: a mutable reference to the Writer
   *
   * The Reassembler's job is to reassemble the indexed substrings (possibly
   * out-of-order and possibly overlapping) back into the original ByteStream.
   * As soon as the Reassembler learns the next byte in the stream, it should
   * write it to the output.
   *
   * If the Reassembler learns about bytes that fit within the stream's
   * available capacity but can't yet be written (because earlier bytes remain
   * unknown), it should store them internally until the gaps are filled in.
   *
   * The Reassembler should discard any bytes that lie beyond the stream's
   * available capacity (i.e., bytes that couldn't be written even if earlier
   * gaps get filled in).
   *
   * The Reassembler should close the stream after writing the last byte.
   */
  void insert( ByteStreamIndex first_index, std::string data, bool is_last_substring );

  // How many bytes are stored in the Reassembler itself?
  // This function is for testing only; don't add extra state to support it.
  uint64_t count_bytes_pending() const;

  // Access output stream reader
  Reader& reader() { return output_.reader(); }
  const Reader& reader() const { return output_.reader(); }

  // Access output stream writer, but const-only (can't write from outside)
  const Writer& writer() const { return output_.writer(); }

private:
  Writer& GetWriter() { return output_.writer(); }

  void UpdatePending( ByteStreamIndex data_first, std::string data );

  ByteStreamIndex FirstUnassembled() const noexcept { return writer().bytes_pushed(); }

  ByteStreamIndex FirstUnaccepted() const noexcept { return FirstUnassembled() + writer().available_capacity(); }

  std::optional<char>& PendingAt( ByteStreamIndex i ) { return pending_[i - FirstUnassembled()]; }

  std::optional<char> PendingAt( ByteStreamIndex i ) const { return pending_[i - FirstUnassembled()]; }

  void TryPushToByteStream();

  ByteStream output_;
  ByteStreamIndex capacity_;
  std::vector<std::optional<char>> pending_;
  std::optional<ByteStreamIndex> index_of_after_final_byte_ { std::nullopt };
};

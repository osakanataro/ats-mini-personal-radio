#ifndef PARSER_TYPES_H
#define PARSER_TYPES_H

#include <cstddef>
#include <cstdint>

// Memory slot names are UTF-8. The character limit keeps a full-width name
// inside the display area reserved for the station name, and the byte limit
// holds that many three byte characters plus the terminator. Declared here so
// the protocol parser, which also builds for the host unit tests, shares it
// with the firmware.
#define MEMORY_NAME_CHARS 9
#define MEMORY_NAME_SIZE  (MEMORY_NAME_CHARS * 3 + 1)

// The same limit spelled out, for user-facing messages and HTML attributes
#define MEMORY_NAME_STRINGIFY_(x) #x
#define MEMORY_NAME_STRINGIFY(x)  MEMORY_NAME_STRINGIFY_(x)
#define MEMORY_NAME_CHARS_STR     MEMORY_NAME_STRINGIFY(MEMORY_NAME_CHARS)

// Validate UTF-8 text for a memory slot name: printable, no control
// characters, and limited to the Basic Multilingual Plane, which is all the
// bundled Japanese font covers. Four byte sequences are valid UTF-8 but have
// no glyphs, so they are rejected rather than drawn as blanks. Returns the
// number of characters, or -1 when the text is malformed, out of range, or
// longer than maxChars. Inline here so the firmware and the protocol parser,
// which also builds for the host unit tests, share one implementation.
inline int utf8Validate(const char *text, size_t bytes, int maxChars)
{
  int chars = 0;

  for(size_t i=0 ; i<bytes ; )
  {
    const unsigned char lead = (unsigned char)text[i];
    size_t length;

    if(lead<0x20 || lead==0x7f) return(-1);     // Control characters
    else if(lead<0x80) length = 1;
    else if(lead>=0xc2 && lead<=0xdf) length = 2;
    else if(lead>=0xe0 && lead<=0xef) length = 3;
    else return(-1);                            // Continuation or 4+ byte lead

    if(i+length>bytes) return(-1);              // Truncated sequence

    for(size_t j=1 ; j<length ; j++)
      if(((unsigned char)text[i+j] & 0xc0) != 0x80) return(-1);

    // Reject overlong forms and UTF-16 surrogates
    if(length==3)
    {
      const unsigned char next = (unsigned char)text[i+1];
      if(lead==0xe0 && next<0xa0) return(-1);
      if(lead==0xed && next>=0xa0) return(-1);
    }

    if(++chars > maxChars) return(-1);
    i += length;
  }

  return(chars);
}

// True when the text needs the Japanese font, i.e. it is not plain ASCII
inline bool utf8IsWide(const char *text)
{
  for( ; *text ; text++)
    if((unsigned char)*text & 0x80) return(true);

  return(false);
}

enum class ParseResult : uint8_t {
  None, // No event: partial input, ignored bytes, or ongoing discard.
  Ready, BlankFrame, DiscardComplete, FrameTooLong, UnknownCommand,
  InvalidSyntax,   // Wrong framing, argument count, or required command keywords.
  InvalidArgument, // Malformed, oversized, or out-of-range argument value.
  OutOfRange, Unsupported
};

struct ByteSpan {
  const uint8_t* data;
  size_t size;
  ByteSpan(const uint8_t* data = nullptr, size_t size = 0) : data(data), size(size) {}
};

namespace command_parser {
// Framers inspect nonempty input without changing parser state or output.
enum class FramingDecision { Incomplete, Complete, BlankFrame, Ignore, RestartAtCurrentByte };
}

// Inspect receives nonempty buffered input. A null terminator means recovery
// discards until reset(), suitable for protocols without a delimiter.
struct FramingRules {
  command_parser::FramingDecision (*inspect)(ByteSpan input);
  bool (*isTerminator)(uint8_t byte);
};

#endif

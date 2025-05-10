#ifndef MINNOW_SERIALIZABLE_HH
#define MINNOW_SERIALIZABLE_HH

#include "parser.hh"

class Serializable {
  public:
    virtual void serialize(Serializer& serializer) const = 0;
    virtual ~Serializable() = default;
};

# endif // MINNOW_SERIALIZABLE_HH
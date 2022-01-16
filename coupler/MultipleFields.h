
#pragma once

#include "pam_const.h"

// Aggregate multiple fields into a single field that makes it easier to operate
// on them together inside the same kernel. used mostly for tracers
template <int MAX_FIELDS, class T>
class MultipleFields {
public:
  yakl::SArray<T,1,MAX_FIELDS> fields;
  int num_fields;

  YAKL_INLINE MultipleFields() { num_fields = 0; }

  YAKL_INLINE MultipleFields(MultipleFields const &rhs) {
    this->num_fields = rhs.num_fields;
    for (int i=0; i < num_fields; i++) {
      this->fields(i) = rhs.fields(i);
    }
  }

  YAKL_INLINE MultipleFields & operator=(MultipleFields const &rhs) {
    this->num_fields = rhs.num_fields;
    for (int i=0; i < num_fields; i++) {
      this->fields(i) = rhs.fields(i);
    }
    return *this;
  }

  YAKL_INLINE MultipleFields(MultipleFields &&rhs) {
    this->num_fields = rhs.num_fields;
    for (int i=0; i < num_fields; i++) {
      this->fields(i) = rhs.fields(i);
    }
  }

  YAKL_INLINE MultipleFields& operator=(MultipleFields &&rhs) {
    this->num_fields = rhs.num_fields;
    for (int i=0; i < num_fields; i++) {
      this->fields(i) = rhs.fields(i);
    }
    return *this;
  }

  YAKL_INLINE void add_field( T &field ) {
    this->fields(num_fields) = field;
    num_fields++;
  }

  YAKL_INLINE auto operator() (int tr, int i1) const -> decltype(fields(tr)(i1)) {
    return this->fields(tr)(i1);
  }
  YAKL_INLINE auto operator() (int tr, int i1, int i2) const -> decltype(fields(tr)(i1,i2)) {
    return this->fields(tr)(i1,i2);
  }
  YAKL_INLINE auto operator() (int tr, int i1, int i2, int i3) const -> decltype(fields(tr)(i1,i2,i3)) {
    return this->fields(tr)(i1,i2,i3);
  }
  YAKL_INLINE auto operator() (int tr, int i1, int i2, int i3, int i4) const -> decltype(fields(tr)(i1,i2,i3,i4)) {
    return this->fields(tr)(i1,i2,i3,i4);
  }
};



template <class T, int N>
using MultiField = MultipleFields< max_fields , Array<T,N,memDevice,styleC> >;




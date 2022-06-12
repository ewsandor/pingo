#ifndef __HILBERT_LUT_HPP__
#define __HILBERT_LUT_HPP__

#include "hilbert.hpp"

namespace sandor_laboratories
{
  namespace pingo
  {
    #define MAX_INDEX_LUT_MAX 17
    extern const hilbert_index_t      max_index_lut[MAX_INDEX_LUT_MAX];
    #define MAX_COORDINATE_LUT_MAX 17
    extern const hilbert_coordinate_t max_coordinate_lut[MAX_COORDINATE_LUT_MAX];

    extern const hilbert_coordinate_s hilbert_coordinate_order_1[4];
    extern const hilbert_coordinate_s hilbert_coordinate_order_2[16];
    extern const hilbert_coordinate_s hilbert_coordinate_order_3[64];
    extern const hilbert_coordinate_s hilbert_coordinate_order_4[256];
    extern const hilbert_coordinate_s hilbert_coordinate_order_8[65536];
  }
}

#endif /* __HILBERT_LUT_HPP__ */
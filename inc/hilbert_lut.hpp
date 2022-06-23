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

    #define HILBERT_COORDINATE_ORDER_1           1
    #define HILBERT_COORDINATE_ORDER_1_MAX_INDEX 4
    extern const hilbert_coordinate_s hilbert_coordinate_order_1[HILBERT_COORDINATE_ORDER_1_MAX_INDEX];
    #define HILBERT_COORDINATE_ORDER_2           2
    #define HILBERT_COORDINATE_ORDER_2_MAX_INDEX 16
    extern const hilbert_coordinate_s hilbert_coordinate_order_2[HILBERT_COORDINATE_ORDER_2_MAX_INDEX];
    #define HILBERT_COORDINATE_ORDER_3           3
    #define HILBERT_COORDINATE_ORDER_3_MAX_INDEX 64
    extern const hilbert_coordinate_s hilbert_coordinate_order_3[HILBERT_COORDINATE_ORDER_3_MAX_INDEX];
    #define HILBERT_COORDINATE_ORDER_4           4
    #define HILBERT_COORDINATE_ORDER_4_MAX_INDEX 256
    extern const hilbert_coordinate_s hilbert_coordinate_order_4[HILBERT_COORDINATE_ORDER_4_MAX_INDEX];
    #define HILBERT_COORDINATE_ORDER_8           8
    #define HILBERT_COORDINATE_ORDER_8_MAX_INDEX 65536
    extern const hilbert_coordinate_s hilbert_coordinate_order_8[HILBERT_COORDINATE_ORDER_8_MAX_INDEX];
  }
}

#endif /* __HILBERT_LUT_HPP__ */
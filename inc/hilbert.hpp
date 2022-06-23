#ifndef __HILBERT_HPP__
#define __HILBERT_HPP__

#include <cinttypes>

namespace sandor_laboratories
{
  namespace pingo
  {
    typedef unsigned int  hilbert_order_t;
    typedef uint_fast64_t hilbert_index_t;
    typedef unsigned int  hilbert_coordinate_t;

    typedef struct
    {
      hilbert_coordinate_t x;
      hilbert_coordinate_t y;

    } hilbert_coordinate_s;

    typedef enum
    {
      HILBERT_ORIENTATION_A,
      HILBERT_ORIENTATION_B,
      HILBERT_ORIENTATION_C,
      HILBERT_ORIENTATION_D,
      HILBERT_ORIENTATION_MAX,

    } hilbert_orientation_e;

    class hilbert_curve_c
    {
      private:
        const hilbert_order_t order; 

        static bool orientate_hilbert_coordinate( const hilbert_coordinate_t max_coordinate, const hilbert_orientation_e orientation, 
                                                  const hilbert_coordinate_s coordinate_in,        hilbert_coordinate_s *coordinate_out);
        static void get_coordinate_next_order(hilbert_order_t order, hilbert_index_t index, hilbert_coordinate_s *coordinate);
        static bool get_coordinate_from_lut  (hilbert_order_t order, hilbert_index_t index, hilbert_coordinate_s *coordinate);

      public:
        hilbert_curve_c(hilbert_order_t order);

        static bool get_coordinate(hilbert_order_t order, hilbert_index_t index, hilbert_coordinate_s *coordinate);
        bool        get_coordinate(hilbert_index_t index, hilbert_coordinate_s *coordinate) const {return get_coordinate(order, index, coordinate);};

        static hilbert_index_t      max_index(hilbert_order_t order);
        hilbert_index_t             max_index() const {return max_index(order);};
        static hilbert_coordinate_t max_coordinate(hilbert_order_t order);
        hilbert_coordinate_t        max_coordinate() const {return max_coordinate(order);};
        hilbert_order_t             get_order() const {return order;};
    };
  }
}

#endif /* __HILBERT_HPP__ */
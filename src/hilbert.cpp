#include <cassert>
#include <cstdio>
#include <cstring>

#include "hilbert.hpp"
#include "hilbert_lut.hpp"

using namespace sandor_laboratories::pingo;

hilbert_curve_c::hilbert_curve_c(hilbert_order_t order_) : order(order_)
{
  if(0 == order)
  {
    fprintf(stderr, "Order 0 Hilbert Curve is not supported.\n");
  }
}

hilbert_index_t hilbert_curve_c::max_index(hilbert_order_t order)
{
  if(order < MAX_INDEX_LUT_MAX)
  {
    return max_index_lut[order];
  }

  return (max_index_lut[(MAX_INDEX_LUT_MAX-1)] << (2*(1+(order - MAX_INDEX_LUT_MAX))));
}

hilbert_coordinate_t hilbert_curve_c::max_coordinate(hilbert_order_t order)
{
  if(order < MAX_COORDINATE_LUT_MAX)
  {
    return max_coordinate_lut[order];
  }

  return (max_coordinate_lut[(MAX_COORDINATE_LUT_MAX-1)] << (1+(order - MAX_COORDINATE_LUT_MAX)));
}

inline bool hilbert_curve_c::orientate_hilbert_coordinate( const hilbert_coordinate_t max_coordinate, const hilbert_orientation_e orientation, 
                                                    const hilbert_coordinate_s coordinate_in,        hilbert_coordinate_s *coordinate_out)
{
  bool ret_val = true;

  if(coordinate_out != nullptr)
  {
    if( (coordinate_in.x < max_coordinate) &&
        (coordinate_in.y < max_coordinate) )
    {
      switch(orientation)
      {
        case HILBERT_ORIENTATION_A:
        {
          // .___.
          // |   |
          // .   V
          coordinate_out->x = coordinate_in.x;
          coordinate_out->y = coordinate_in.y;
          break;
        }
        case HILBERT_ORIENTATION_B:
        {
          // .___.
          // |
          // .--->
          coordinate_out->x = (max_coordinate-1)-coordinate_in.y;
          coordinate_out->y = (max_coordinate-1)-coordinate_in.x;
          break;
        }
        case HILBERT_ORIENTATION_C:
        {
          // ^   .
          // |   |
          // .___.
          coordinate_out->x = (max_coordinate-1)-coordinate_in.x;
          coordinate_out->y = (max_coordinate-1)-coordinate_in.y;
          break;
        }
        case HILBERT_ORIENTATION_D:
        {
          // <---.
          //     |
          // .___.
          coordinate_out->x = coordinate_in.y;
          coordinate_out->y = coordinate_in.x;
          break;
        }
        default:
        {
          fprintf(stderr, "Invalid orientation %u\n", orientation);
          ret_val = false;
          break;
        }
      }
    }
    else
    {
      fprintf(stderr, "Input coordinate out of bounds.  coordinate_in (%u, %u) max_coordinate %u\n", coordinate_in.x, coordinate_in.y, max_coordinate);
      ret_val = false;
    }
  }
  else
  {
    fprintf(stderr, "Null output coordinate\n");
    ret_val = false;
  }

  return ret_val;
}

inline void hilbert_curve_c::get_coordinate_next_order(hilbert_order_t order, hilbert_index_t index, hilbert_coordinate_s *coordinate)
{
  const hilbert_coordinate_t max_coordinate_value = max_coordinate(order);
  const hilbert_index_t      next_order_max_index = max_index(order-1);

  assert(coordinate != nullptr);
  assert(get_coordinate((order-1), (index % next_order_max_index), coordinate));

  switch(index / next_order_max_index)
  {
    case 0:
    {
      assert(orientate_hilbert_coordinate(max_coordinate_value/2, HILBERT_ORIENTATION_D, *coordinate, coordinate));
      //coordinate->x += 0;
      //coordinate->y += 0;
      break;
    }
    case 1:
    {
      assert(orientate_hilbert_coordinate(max_coordinate_value/2, HILBERT_ORIENTATION_A, *coordinate, coordinate));
      //coordinate->x += 0;
      coordinate->y += max_coordinate_value/2;
      break;
    }
    case 2:
    {
      assert(orientate_hilbert_coordinate(max_coordinate_value/2, HILBERT_ORIENTATION_A, *coordinate, coordinate));
      coordinate->x += max_coordinate_value/2;
      coordinate->y += max_coordinate_value/2;
      break;
    }
    case 3:
    {
      assert(orientate_hilbert_coordinate(max_coordinate_value/2, HILBERT_ORIENTATION_B, *coordinate, coordinate));
      coordinate->x += max_coordinate_value/2;
      //coordinate->y += 0;
      break;
    }
    default:
    {
      /* Unexpected */
      fprintf(stderr, "Order %u index %lu next_order_max_index %lu\n", order, index, next_order_max_index);
      assert(0);
      break;
    }
  }
}

inline bool hilbert_curve_c::get_coordinate_from_lut(hilbert_order_t order, const hilbert_index_t index, hilbert_coordinate_s *coordinate)
{
  bool ret_val = true;

  if(HILBERT_COORDINATE_ORDER_1 == order)
  {
    assert(index < HILBERT_COORDINATE_ORDER_1_MAX_INDEX);
    *coordinate = hilbert_coordinate_order_1[index];
  }
  else if(HILBERT_COORDINATE_ORDER_2 == order)
  {
    assert(index < HILBERT_COORDINATE_ORDER_2_MAX_INDEX);
    *coordinate = hilbert_coordinate_order_2[index];
  }
  else if(HILBERT_COORDINATE_ORDER_3 == order)
  {
    assert(index < HILBERT_COORDINATE_ORDER_3_MAX_INDEX);
    *coordinate = hilbert_coordinate_order_3[index];
  }
  else if(HILBERT_COORDINATE_ORDER_4 == order)
  {
    assert(index < HILBERT_COORDINATE_ORDER_4_MAX_INDEX);
    *coordinate = hilbert_coordinate_order_4[index];
  }
  else if(HILBERT_COORDINATE_ORDER_8 == order)
  {
    assert(index < HILBERT_COORDINATE_ORDER_8_MAX_INDEX);
    *coordinate = hilbert_coordinate_order_8[index];
  }
  else
  {
    ret_val = false;
  }

  return ret_val;
}

bool hilbert_curve_c::get_coordinate(hilbert_order_t order, hilbert_index_t index, hilbert_coordinate_s *coordinate)
{
  bool ret_val = true;

  if(coordinate != nullptr)
  {
    const hilbert_index_t      max_index_value      = max_index(order);
    const hilbert_coordinate_t max_coordinate_value = max_coordinate(order);
    if(index < max_index_value)
    {
      if(!get_coordinate_from_lut(order, index, coordinate))
      {
        if (order > 0)
        {
          get_coordinate_next_order(order, index, coordinate);
        }
        else
        {
          fprintf(stderr, "Order 0 Hilbert Curve is not supported.\n");
          *coordinate = {.x = max_coordinate_value, .y = max_coordinate_value};
          ret_val = false;
        }
      }
    }
    else
    {
      fprintf(stderr, "Index %lu exceeds max_index_value %lu\n", index, max_index_value);
      *coordinate = {.x = max_coordinate_value, .y = max_coordinate_value};
      ret_val = false;
    }
  }
  else
  {
    fprintf(stderr, "Null output coordinate\n");
    ret_val = false;
  }

  return ret_val;
}
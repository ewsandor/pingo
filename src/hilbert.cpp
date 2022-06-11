#include <cassert>
#include <cstdio>
#include <cstring>

#include "hilbert.hpp"

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
  hilbert_index_t ret_val = ((order > 0)?4:0);

  for(unsigned int i = 1; i < order; i++)
  {
    ret_val *= 4;
  }

  return ret_val;
}

hilbert_coordinate_t hilbert_curve_c::max_coordinate(hilbert_order_t order)
{
  hilbert_coordinate_t ret_val = ((order > 0)?2:0);

  for(unsigned int i = 1; i < order; i++)
  {
    ret_val *= 2;
  }

  return ret_val;
}

bool hilbert_curve_c::orientate_hilbert_coordinate( const hilbert_coordinate_t max_coordinate, const hilbert_orientation_e orientation, 
                                                    const hilbert_coordinate_s coordinate_in,        hilbert_coordinate_s *coordinate_out)
{
  bool ret_val = true;

  if(coordinate_out)
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


static const hilbert_coordinate_s hilbert_coordinate_order_1[4] = 
  { {.x = 0, .y = 0}, {.x = 0, .y = 1}, {.x = 1, .y = 1}, {.x = 1, .y = 0} };
static const hilbert_coordinate_s hilbert_coordinate_order_2[16] = 
  {
    {.x = 0, .y = 0}, {.x = 1, .y = 0}, {.x = 1, .y = 1}, {.x = 0, .y = 1},
    {.x = 0, .y = 2}, {.x = 0, .y = 3}, {.x = 1, .y = 3}, {.x = 1, .y = 2},
    {.x = 2, .y = 2}, {.x = 2, .y = 3}, {.x = 3, .y = 3}, {.x = 3, .y = 2},
    {.x = 3, .y = 1}, {.x = 2, .y = 1}, {.x = 2, .y = 0}, {.x = 3, .y = 0}
  };
static const hilbert_coordinate_s hilbert_coordinate_order_3[64] = 
  {
    {.x = 0, .y = 0}, {.x = 0, .y = 1}, {.x = 1, .y = 1}, {.x = 1, .y = 0},
    {.x = 2, .y = 0}, {.x = 3, .y = 0}, {.x = 3, .y = 1}, {.x = 2, .y = 1},
    {.x = 2, .y = 2}, {.x = 3, .y = 2}, {.x = 3, .y = 3}, {.x = 2, .y = 3},
    {.x = 1, .y = 3}, {.x = 1, .y = 2}, {.x = 0, .y = 2}, {.x = 0, .y = 3},
    {.x = 0, .y = 4}, {.x = 1, .y = 4}, {.x = 1, .y = 5}, {.x = 0, .y = 5},
    {.x = 0, .y = 6}, {.x = 0, .y = 7}, {.x = 1, .y = 7}, {.x = 1, .y = 6},
    {.x = 2, .y = 6}, {.x = 2, .y = 7}, {.x = 3, .y = 7}, {.x = 3, .y = 6},
    {.x = 3, .y = 5}, {.x = 2, .y = 5}, {.x = 2, .y = 4}, {.x = 3, .y = 4},
    {.x = 4, .y = 4}, {.x = 5, .y = 4}, {.x = 5, .y = 5}, {.x = 4, .y = 5},
    {.x = 4, .y = 6}, {.x = 4, .y = 7}, {.x = 5, .y = 7}, {.x = 5, .y = 6},
    {.x = 6, .y = 6}, {.x = 6, .y = 7}, {.x = 7, .y = 7}, {.x = 7, .y = 6},
    {.x = 7, .y = 5}, {.x = 6, .y = 5}, {.x = 6, .y = 4}, {.x = 7, .y = 4},
    {.x = 7, .y = 3}, {.x = 7, .y = 2}, {.x = 6, .y = 2}, {.x = 6, .y = 3},
    {.x = 5, .y = 3}, {.x = 4, .y = 3}, {.x = 4, .y = 2}, {.x = 5, .y = 2},
    {.x = 5, .y = 1}, {.x = 4, .y = 1}, {.x = 4, .y = 0}, {.x = 5, .y = 0},
    {.x = 6, .y = 0}, {.x = 6, .y = 1}, {.x = 7, .y = 1}, {.x = 7, .y = 0},
  };

static const hilbert_coordinate_s hilbert_coordinate_order_4[256] = 
  {
    {.x = 0, .y = 0},   {.x = 1, .y = 0},   {.x = 1, .y = 1},   {.x = 0, .y = 1},
    {.x = 0, .y = 2},   {.x = 0, .y = 3},   {.x = 1, .y = 3},   {.x = 1, .y = 2},
    {.x = 2, .y = 2},   {.x = 2, .y = 3},   {.x = 3, .y = 3},   {.x = 3, .y = 2},
    {.x = 3, .y = 1},   {.x = 2, .y = 1},   {.x = 2, .y = 0},   {.x = 3, .y = 0},
    {.x = 4, .y = 0},   {.x = 4, .y = 1},   {.x = 5, .y = 1},   {.x = 5, .y = 0},
    {.x = 6, .y = 0},   {.x = 7, .y = 0},   {.x = 7, .y = 1},   {.x = 6, .y = 1},
    {.x = 6, .y = 2},   {.x = 7, .y = 2},   {.x = 7, .y = 3},   {.x = 6, .y = 3},
    {.x = 5, .y = 3},   {.x = 5, .y = 2},   {.x = 4, .y = 2},   {.x = 4, .y = 3},
    {.x = 4, .y = 4},   {.x = 4, .y = 5},   {.x = 5, .y = 5},   {.x = 5, .y = 4},
    {.x = 6, .y = 4},   {.x = 7, .y = 4},   {.x = 7, .y = 5},   {.x = 6, .y = 5},
    {.x = 6, .y = 6},   {.x = 7, .y = 6},   {.x = 7, .y = 7},   {.x = 6, .y = 7},
    {.x = 5, .y = 7},   {.x = 5, .y = 6},   {.x = 4, .y = 6},   {.x = 4, .y = 7},
    {.x = 3, .y = 7},   {.x = 2, .y = 7},   {.x = 2, .y = 6},   {.x = 3, .y = 6},
    {.x = 3, .y = 5},   {.x = 3, .y = 4},   {.x = 2, .y = 4},   {.x = 2, .y = 5},
    {.x = 1, .y = 5},   {.x = 1, .y = 4},   {.x = 0, .y = 4},   {.x = 0, .y = 5},
    {.x = 0, .y = 6},   {.x = 1, .y = 6},   {.x = 1, .y = 7},   {.x = 0, .y = 7},
    {.x = 0, .y = 8},   {.x = 0, .y = 9},   {.x = 1, .y = 9},   {.x = 1, .y = 8},
    {.x = 2, .y = 8},   {.x = 3, .y = 8},   {.x = 3, .y = 9},   {.x = 2, .y = 9},
    {.x = 2, .y = 10},  {.x = 3, .y = 10},  {.x = 3, .y = 11},  {.x = 2, .y = 11},
    {.x = 1, .y = 11},  {.x = 1, .y = 10},  {.x = 0, .y = 10},  {.x = 0, .y = 11},
    {.x = 0, .y = 12},  {.x = 1, .y = 12},  {.x = 1, .y = 13},  {.x = 0, .y = 13},
    {.x = 0, .y = 14},  {.x = 0, .y = 15},  {.x = 1, .y = 15},  {.x = 1, .y = 14},
    {.x = 2, .y = 14},  {.x = 2, .y = 15},  {.x = 3, .y = 15},  {.x = 3, .y = 14},
    {.x = 3, .y = 13},  {.x = 2, .y = 13},  {.x = 2, .y = 12},  {.x = 3, .y = 12},
    {.x = 4, .y = 12},  {.x = 5, .y = 12},  {.x = 5, .y = 13},  {.x = 4, .y = 13},
    {.x = 4, .y = 14},  {.x = 4, .y = 15},  {.x = 5, .y = 15},  {.x = 5, .y = 14},
    {.x = 6, .y = 14},  {.x = 6, .y = 15},  {.x = 7, .y = 15},  {.x = 7, .y = 14},
    {.x = 7, .y = 13},  {.x = 6, .y = 13},  {.x = 6, .y = 12},  {.x = 7, .y = 12},
    {.x = 7, .y = 11},  {.x = 7, .y = 10},  {.x = 6, .y = 10},  {.x = 6, .y = 11},
    {.x = 5, .y = 11},  {.x = 4, .y = 11},  {.x = 4, .y = 10},  {.x = 5, .y = 10},
    {.x = 5, .y = 9},   {.x = 4, .y = 9},   {.x = 4, .y = 8},   {.x = 5, .y = 8},
    {.x = 6, .y = 8},   {.x = 6, .y = 9},   {.x = 7, .y = 9},   {.x = 7, .y = 8},
    {.x = 8, .y = 8},   {.x = 8, .y = 9},   {.x = 9, .y = 9},   {.x = 9, .y = 8},
    {.x = 10, .y = 8},  {.x = 11, .y = 8},  {.x = 11, .y = 9},  {.x = 10, .y = 9},
    {.x = 10, .y = 10}, {.x = 11, .y = 10}, {.x = 11, .y = 11}, {.x = 10, .y = 11},
    {.x = 9, .y = 11},  {.x = 9, .y = 10},  {.x = 8, .y = 10},  {.x = 8, .y = 11},
    {.x = 8, .y = 12},  {.x = 9, .y = 12},  {.x = 9, .y = 13},  {.x = 8, .y = 13},
    {.x = 8, .y = 14},  {.x = 8, .y = 15},  {.x = 9, .y = 15},  {.x = 9, .y = 14},
    {.x = 10, .y = 14}, {.x = 10, .y = 15}, {.x = 11, .y = 15}, {.x = 11, .y = 14},
    {.x = 11, .y = 13}, {.x = 10, .y = 13}, {.x = 10, .y = 12}, {.x = 11, .y = 12},
    {.x = 12, .y = 12}, {.x = 13, .y = 12}, {.x = 13, .y = 13}, {.x = 12, .y = 13},
    {.x = 12, .y = 14}, {.x = 12, .y = 15}, {.x = 13, .y = 15}, {.x = 13, .y = 14},
    {.x = 14, .y = 14}, {.x = 14, .y = 15}, {.x = 15, .y = 15}, {.x = 15, .y = 14},
    {.x = 15, .y = 13}, {.x = 14, .y = 13}, {.x = 14, .y = 12}, {.x = 15, .y = 12},
    {.x = 15, .y = 11}, {.x = 15, .y = 10}, {.x = 14, .y = 10}, {.x = 14, .y = 11},
    {.x = 13, .y = 11}, {.x = 12, .y = 11}, {.x = 12, .y = 10}, {.x = 13, .y = 10},
    {.x = 13, .y = 9},  {.x = 12, .y = 9},  {.x = 12, .y = 8},  {.x = 13, .y = 8},
    {.x = 14, .y = 8},  {.x = 14, .y = 9},  {.x = 15, .y = 9},  {.x = 15, .y = 8},
    {.x = 15, .y = 7},  {.x = 14, .y = 7},  {.x = 14, .y = 6},  {.x = 15, .y = 6},
    {.x = 15, .y = 5},  {.x = 15, .y = 4},  {.x = 14, .y = 4},  {.x = 14, .y = 5},
    {.x = 13, .y = 5},  {.x = 13, .y = 4},  {.x = 12, .y = 4},  {.x = 12, .y = 5},
    {.x = 12, .y = 6},  {.x = 13, .y = 6},  {.x = 13, .y = 7},  {.x = 12, .y = 7},
    {.x = 11, .y = 7},  {.x = 11, .y = 6},  {.x = 10, .y = 6},  {.x = 10, .y = 7},
    {.x = 9, .y = 7},   {.x = 8, .y = 7},   {.x = 8, .y = 6},   {.x = 9, .y = 6},
    {.x = 9, .y = 5},   {.x = 8, .y = 5},   {.x = 8, .y = 4},   {.x = 9, .y = 4},
    {.x = 10, .y = 4},  {.x = 10, .y = 5},  {.x = 11, .y = 5},  {.x = 11, .y = 4},
    {.x = 11, .y = 3},  {.x = 11, .y = 2},  {.x = 10, .y = 2},  {.x = 10, .y = 3},
    {.x = 9, .y = 3},   {.x = 8, .y = 3},   {.x = 8, .y = 2},   {.x = 9, .y = 2},
    {.x = 9, .y = 1},   {.x = 8, .y = 1},   {.x = 8, .y = 0},   {.x = 9, .y = 0},
    {.x = 10, .y = 0},  {.x = 10, .y = 1},  {.x = 11, .y = 1},  {.x = 11, .y = 0},
    {.x = 12, .y = 0},  {.x = 13, .y = 0},  {.x = 13, .y = 1},  {.x = 12, .y = 1},
    {.x = 12, .y = 2},  {.x = 12, .y = 3},  {.x = 13, .y = 3},  {.x = 13, .y = 2},
    {.x = 14, .y = 2},  {.x = 14, .y = 3},  {.x = 15, .y = 3},  {.x = 15, .y = 2},
    {.x = 15, .y = 1},  {.x = 14, .y = 1},  {.x = 14, .y = 0},  {.x = 15, .y = 0}
  };

bool hilbert_curve_c::get_coordinate(hilbert_order_t order, hilbert_index_t index, hilbert_coordinate_s *coordinate)
{
  bool ret_val = true;

  if(coordinate)
  {
    const hilbert_index_t      max_index_value      = max_index(order);
    const hilbert_coordinate_t max_coordinate_value = max_coordinate(order);
    if(index < max_index_value)
    {
      if(1 == order)
      {
        assert(index < 4);
        *coordinate = hilbert_coordinate_order_1[index];
      }
      else if(2 == order)
      {
        assert(index < 16);
        *coordinate = hilbert_coordinate_order_2[index];
      }
      else if(3 == order)
      {
        assert(index < 64);
        *coordinate = hilbert_coordinate_order_3[index];
      }
      else if(4 == order)
      {
        assert(index < 256);
        *coordinate = hilbert_coordinate_order_4[index];
      }
       else if (order > 2)
      {
        const hilbert_index_t next_order_max_index = max_index(order-1);
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
      else
      {
        fprintf(stderr, "Order 0 Hilbert Curve is not supported.\n");
        *coordinate = {.x = max_coordinate_value, .y = max_coordinate_value};
        ret_val = false;
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
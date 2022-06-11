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

bool hilbert_curve_c::get_coordinate(hilbert_order_t order, hilbert_index_t index, hilbert_coordinate_s *coordinate)
{
  bool ret_val = true;

  if(coordinate)
  {
    const hilbert_coordinate_t max_coordinate_value = max_coordinate(order);
    memset(coordinate, 0, sizeof(hilbert_coordinate_s));

    if(1 == order)
    {
      switch(index % 4)
      {
        case 0:
        {
          *coordinate = {.x = 0, .y = 0};
          break;
        }
        case 1:
        {
          *coordinate = {.x = 0, .y = 1};
          break;
        }
        case 2:
        {
          *coordinate = {.x = 1, .y = 1};
          break;
        }
        case 3:
        {
          *coordinate = {.x = 1, .y = 0};
          break;
        }
        default:
        {
          assert(0);
          break;
        }
      }
    }
    else if (order > 1)
    {
      assert(get_coordinate((order-1), index, coordinate));
      /* TODO - rotate and offset */
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
    fprintf(stderr, "Null output coordinate\n");
    ret_val = false;
  }

  return ret_val;
}
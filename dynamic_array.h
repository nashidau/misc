#pragma once

#define DYN_ARR_OF(type) struct  { \
  type *data; \
  type *endptr; \
  uint32_t capacity; \
}

#if !defined(__cplusplus)
#define decltype(x) void*
#endif

#define DYN_ARR_RESET(a, c) { \
  a.data = (decltype(a.data))malloc(sizeof(a.data[0]) * c); \
  a.endptr = a.data; \
  a.capacity = c; \
}

#define DYN_ARR_RESIZE(a, s) { \
  uint32_t size = (s); \
  if (a.capacity < size) { \
    a.data = (decltype(a.data))realloc(a.data, sizeof(a.data[0]) * size); \
    a.capacity = size; \
  } \
  a.endptr = a.data + size; \
} 

#define DYN_ARR_DESTROY(a) if(a.data != NULL) { \
  free(a.data); \
  a.data = a.endptr = NULL; \
}

#define DYN_ARR_APPEND(a, v) { \
  ptrdiff_t cur_size = a.endptr - a.data; \
  assert(cur_size >= 0);                                 \
  if ((size_t)cur_size >= a.capacity) { \
    a.capacity <<= 1u; \
    decltype(a.data) tmp = (decltype(a.data)) realloc(a.data, sizeof(a.data[0]) * a.capacity); \
    assert(tmp != NULL); \
    a.data = tmp; \
    a.endptr = &a.data[cur_size]; \
  } \
  *(a.endptr++) = v; \
}

#define DYN_ARR_CLEAR(a) (a.endptr = a.data)
#define DYN_ARR_SIZE(a) ((uint32_t)(a.endptr - a.data))
#define DYN_ARR_AT(a, i) (a.data[i])

#define DYN_ARR_POP(a) {\
  assert(a.data != a.endptr); \
  --(a.endptr); \
}

#define DYN_ARR_EMPTY(a) (a.endptr == a.data)

#define DYN_ARR_BACKPTR(a) (a.endptr - 1)

#define DYN_ARR_FOREACH(a, countername) \
for (size_t countername = 0; (countername) < DYN_ARR_SIZE(a); ++(countername))

/*
 Example usage:
  typedef struct point { uint32_t x, y } point;
  void foo() { 
    DYN_ARR_OF(point) points;
    DYN_ARR_RESET(points, 100u);
    uint32_t npoints = 200u;
    for (uint32_t i = 0u; i < npoints; ++i) {
      point p = {i, i * 10u};
      DYN_ARR_APPEND(points, p);
    }
    assert(DYN_ARR_SIZE(points) == 200u);
    
    DYN_ARR_FOREACH(points, i) {
      assert(DYN_ARR_AT(points, i).x == i);
      assert(DYN_ARR_AT(points, i).y == i * 10u);
    }
    DYN_ARR_CLEAR(points);
    assert(DYN_ARR_SIZE(points) == 0u);
    DYN_ARR_DESTROY(points);
  }
*/

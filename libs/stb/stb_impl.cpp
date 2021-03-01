
#if defined(__GNUC__)
# pragma GCC diagnostic ignored "-Wconversion"
# pragma GCC diagnostic ignored "-Wsign-conversion"
# pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_FAILURE_STRINGS
#include "stb_image.h"


// set miniz compress function for stb PNG writer
// source: https://blog.gibson.sh/2015/07/18/comparing-png-compression-ratios-of-stb_image_write-lodepng-miniz-and-libpng/
#include "miniz/miniz.h"

static unsigned char*
my_stbi_zlib_compress( unsigned char *data, int data_len,
                       int *out_len, int quality )
{
  mz_ulong buflen = mz_compressBound(data_len);
  // Note that the returned buffer will be free'd by stbi_write_png*()
  // with STBIW_FREE(), so if you have overridden that (+ STBIW_MALLOC()),
  // adjust the next malloc() call accordingly:
  unsigned char* buf = static_cast<unsigned char*>(malloc(buflen));
  if( buf == NULL
      || mz_compress2(buf, &buflen, data, data_len, quality) != 0 )
  {
      free(buf); // .. yes, this would have to be adjusted as well.
      return NULL;
  }
  *out_len = buflen;
  return buf;
}
#define STBIW_ZLIB_COMPRESS  my_stbi_zlib_compress

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"


#include <array>
#include <algorithm>

template<size_t size, typename T, typename C>
void my_stbrp_sort(T* ptr, std::size_t count, const C& comp) {
  static_assert(sizeof(T) == size);
  const auto begin = static_cast<T*>(ptr);
  const auto end = begin + count;
  std::sort(begin, end, [&](const T& a, const T& b) { return (comp(&a, &b) < 0); });
}
#define STBRP_SORT(PTR, COUNT, SIZE, COMP) my_stbrp_sort<(SIZE)>((PTR), (COUNT), (COMP))

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

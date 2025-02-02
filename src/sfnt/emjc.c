/****************************************************************************
 *
 * emjc.c
 *
 *   EMJC image support
 *
 * Adapted by Michael Tyson from code created by cc4966 (@496_, cc4966.net) on 2018/08/19.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.  By continuing to use, modify, or distribute
 * this file you indicate that you have read the license and
 * understand and accept it fully.
 *
 */


#include <freetype/internal/ftdebug.h>
#include <freetype/internal/ftstream.h>
#include <freetype/tttags.h>
#include FT_CONFIG_STANDARD_LIBRARY_H


#if defined( TT_CONFIG_OPTION_EMBEDDED_BITMAPS ) && \
    defined( FT_CONFIG_OPTION_USE_EMJC )


#include <stdlib.h>
#include <string.h>
#include <lzfse/lzfse.h>

  static uint16_t emjc_width(const uint8_t *__restrict src_buffer)
  {
      const uint8_t *p = src_buffer;
      const uint16_t width = (uint16_t) p[8] + ((uint16_t) p[9] << 8);
      return width;
  }

  static uint16_t emjc_height(const uint8_t *__restrict src_buffer)
  {
      const uint8_t *p = src_buffer;
      const uint16_t height = (uint16_t) p[10] + ((uint16_t) p[11] << 8);
      return height;
  }

  static size_t emjc_decode_buffer_size(const uint8_t *__restrict src_buffer)
  {
      const uint8_t *p = src_buffer;
      const size_t width = (size_t) p[8] + ((size_t) p[9] << 8);
      const size_t height = (size_t) p[10] + ((size_t) p[11] << 8);
      const uint16_t appendix_length = (uint16_t) p[12] + ((uint16_t) p[13] << 8);
      return (width * height * (4 + sizeof(int32_t)) + height + appendix_length);
  }

  static int32_t convert_to_difference(int32_t value, int32_t offset) {
      if (value & 1) {
          return -(value >> 1) - offset;
      } else {
          return (value >> 1) + offset;
      }
  }

  static int32_t filter4_value(int32_t left, int32_t upper) {
      int32_t value = left + upper + 1;
      if (value < 0) {
          return -((-value) / 2);
      } else {
          return value / 2;
      }
  }

  static int emjc_decode_buffer(uint8_t *__restrict dst_buffer,
                                const uint8_t *__restrict src_buffer,
                                size_t src_size)
  {
      const uint8_t *p = src_buffer;
      const size_t data_length = src_size;
      if ( p[0] != 'e' || p[1] != 'm' || p[2] != 'j' || p[3] != '1' ) {
          return -1;
      }
      // const uint16_t version = (uint16_t) p[4] + ((uint16_t) p[5] << 8);
      // const uint16_t unknown = (uint16_t) p[6] + ((uint16_t) p[7] << 8); // 0xa101
      const uint16_t width = (uint16_t) p[8] + ((uint16_t) p[9] << 8);
      const uint16_t height = (uint16_t) p[10] + ((uint16_t) p[11] << 8);
      const uint16_t appendix_length = (uint16_t) p[12] + ((uint16_t) p[13] << 8);
      // const uint16_t padding = (uint16_t) p[14] + ((uint16_t) p[15] << 8);
      const uint16_t filter_length = height;
      const size_t dst_length = (size_t) height * width + filter_length + (size_t) height * width * 3 + appendix_length;
      uint8_t *dst = (uint8_t *) malloc(dst_length + 1);
      const size_t len = lzfse_decode_buffer(dst, dst_length + 1, p + 16, data_length - 16, NULL);
      if ( len != dst_length ) {
          free(dst);
          return -1;
      }
      const int pixels = height * width; // alpha
      const int colors = pixels * 3; // rgb
      const uint8_t *alpha = dst;
      const uint8_t *filters = dst + pixels;
      const uint8_t *rgb = dst + pixels + filter_length;
      const uint8_t *appendix = dst + pixels + filter_length + colors;
      int32_t *buffer = (int32_t *) malloc(colors * sizeof(int32_t));
      memset(buffer, 0, colors * sizeof(int32_t));
      for (int i = 0, offset = 0; i < appendix_length; ++i) {
          offset += appendix[i] / 4;
          if (offset >= colors) {
              break;
          }
          buffer[offset++] = 128 * (appendix[i] % 4);
      }
      for (int y = 0, i = 0; y < height; ++y) {
          for (int x = 0; x < width; ++x, ++i) {
              buffer[i * 3 + 0] = convert_to_difference(rgb[i * 3 + 0], buffer[i * 3 + 0]);
              buffer[i * 3 + 1] = convert_to_difference(rgb[i * 3 + 1], buffer[i * 3 + 1]);
              buffer[i * 3 + 2] = convert_to_difference(rgb[i * 3 + 2], buffer[i * 3 + 2]);
              switch (filters[y]) {
                  case 0:
                      break;
                  case 1:
                      if (x > 0 && y > 0) {
                          int32_t left = buffer[(i - 1) * 3 + 0];
                          int32_t upper = buffer[(i - width) * 3 + 0];
                          int32_t leftUpper = buffer[(i - width - 1) * 3 + 0];
                          if (abs(left - leftUpper) < abs(upper - leftUpper)) {
                              buffer[i * 3 + 0] += upper;
                              buffer[i * 3 + 1] += buffer[(i - width) * 3 + 1];
                              buffer[i * 3 + 2] += buffer[(i - width) * 3 + 2];
                          } else {
                              buffer[i * 3 + 0] += left;
                              buffer[i * 3 + 1] += buffer[(i - 1) * 3 + 1];
                              buffer[i * 3 + 2] += buffer[(i - 1) * 3 + 2];
                          }
                      } else if (x > 0) {
                          buffer[i * 3 + 0] += buffer[(i - 1) * 3 + 0];
                          buffer[i * 3 + 1] += buffer[(i - 1) * 3 + 1];
                          buffer[i * 3 + 2] += buffer[(i - 1) * 3 + 2];
                      } else if (y > 0) {
                          buffer[i * 3 + 0] += buffer[(i - width) * 3 + 0];
                          buffer[i * 3 + 1] += buffer[(i - width) * 3 + 1];
                          buffer[i * 3 + 2] += buffer[(i - width) * 3 + 2];
                      }
                      break;
                  case 2:
                      if (x > 0) {
                          buffer[i * 3 + 0] += buffer[(i - 1) * 3 + 0];
                          buffer[i * 3 + 1] += buffer[(i - 1) * 3 + 1];
                          buffer[i * 3 + 2] += buffer[(i - 1) * 3 + 2];
                      }
                      break;
                  case 3:
                      if (y > 0) {
                          buffer[i * 3 + 0] += buffer[(i - width) * 3 + 0];
                          buffer[i * 3 + 1] += buffer[(i - width) * 3 + 1];
                          buffer[i * 3 + 2] += buffer[(i - width) * 3 + 2];
                      }
                      break;
                  case 4:
                      if (x > 0 && y > 0) {
                          buffer[i * 3 + 0] += filter4_value(buffer[(i - 1) * 3 + 0], buffer[(i - width) * 3 + 0]);
                          buffer[i * 3 + 1] += filter4_value(buffer[(i - 1) * 3 + 1], buffer[(i - width) * 3 + 1]);
                          buffer[i * 3 + 2] += filter4_value(buffer[(i - 1) * 3 + 2], buffer[(i - width) * 3 + 2]);
                      } else if (x > 0) {
                          buffer[i * 3 + 0] += buffer[(i - 1) * 3 + 0];
                          buffer[i * 3 + 1] += buffer[(i - 1) * 3 + 1];
                          buffer[i * 3 + 2] += buffer[(i - 1) * 3 + 2];
                      } else if (y > 0) {
                          buffer[i * 3 + 0] += buffer[(i - width) * 3 + 0];
                          buffer[i * 3 + 1] += buffer[(i - width) * 3 + 1];
                          buffer[i * 3 + 2] += buffer[(i - width) * 3 + 2];
                      }
                      break;
              }
              const int32_t base = buffer[i * 3 + 0];
              const int32_t p = buffer[i * 3 + 1];
              const int32_t q = buffer[i * 3 + 2];
              int32_t r, g, b;
              if (p < 0 && q < 0) {
                  r = base + p / 2 - (q + 1) / 2;
                  g = base + q / 2;
                  b = base - (p + 1) / 2 - (q + 1) / 2;
              } else if (p < 0) {
                  r = base + p / 2 - q / 2;
                  g = base + (q + 1) / 2;
                  b = base - (p + 1) / 2 - q / 2;
              } else if (q < 0) {
                  r = base + (p + 1) / 2 - (q + 1) / 2;
                  g = base + q / 2;
                  b = base - p / 2 - (q + 1) / 2;
              } else {
                  r = base + (p + 1) / 2 - q / 2;
                  g = base + (q + 1) / 2;
                  b = base - p / 2 - q / 2;
              }
              dst_buffer[i * 4 + 0] = b < 0 ? (b % 257) + 257 : (b % 257);
              dst_buffer[i * 4 + 1] = g < 0 ? (g % 257) + 257 : (g % 257);
              dst_buffer[i * 4 + 2] = r < 0 ? (r % 257) + 257 : (r % 257);
              dst_buffer[i * 4 + 3] = alpha[i];
          }
      }
      free(dst);
      free(buffer);
      return 0;
  }

  FT_LOCAL_DEF( FT_Error )
  Load_SBit_EMJC(FT_GlyphSlot     slot,
                 TT_SBit_Metrics  metrics,
                 FT_Byte*         data,
                 FT_UInt          emjc_len )
  {
    FT_Bitmap    *map   = &slot->bitmap;
    
    metrics->width  = (FT_UShort)emjc_width(data);
    metrics->height = (FT_UShort)emjc_height(data);

    map->width      = metrics->width;
    map->rows       = metrics->height;
    map->pixel_mode = FT_PIXEL_MODE_BGRA;
    map->pitch      = (int)( map->width * 4 );
    map->num_grays  = 256;

    FT_ULong  size = map->rows * (FT_ULong)map->pitch;
    FT_Error error = ft_glyphslot_alloc_bitmap( slot, size );
    if ( error ) 
    {
      return error;
    }
    
    int result = emjc_decode_buffer(map->buffer, data, emjc_len);
    if ( result != 0 )
    {
      return FT_THROW( Invalid_File_Format );
    }
    
    return FT_Err_Ok;
  }

#else /* !(TT_CONFIG_OPTION_EMBEDDED_BITMAPS && FT_CONFIG_OPTION_USE_PNG) */

  /* ANSI C doesn't like empty source files */
  typedef int  _emjc_dummy;

#endif /* !(TT_CONFIG_OPTION_EMBEDDED_BITMAPS && FT_CONFIG_OPTION_USE_PNG) */


/* END */

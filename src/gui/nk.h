/**
 * @file nk.h
 * @brief Konfigurasi tunggal untuk library Nuklear.
 *
 * Seluruh modul GUI wajib meng-include header ini, bukan nuklear.h secara
 * langsung, agar makro konfigurasi selalu konsisten di setiap unit kompilasi.
 */

#ifndef RDR_GUI_NK_H
#define RDR_GUI_NK_H

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_UINT_DRAW_INDEX
#include "nuklear.h"

#endif /* RDR_GUI_NK_H */

/****************************************************************************
 *
 * emjc.h
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


#ifndef EMJC_H_
#define EMJC_H_


#include "ttload.h"


FT_BEGIN_HEADER

#ifdef FT_CONFIG_OPTION_USE_EMJC

  FT_LOCAL( FT_Error )
  Load_SBit_EMJC(FT_GlyphSlot     slot,
                 TT_SBit_Metrics  metrics,
                 FT_Byte*         data,
                 FT_UInt          emjc_len );

#endif

FT_END_HEADER

#endif /* EMJC_H_ */


/* END */

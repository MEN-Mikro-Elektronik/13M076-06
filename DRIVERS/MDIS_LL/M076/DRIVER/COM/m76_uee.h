/***********************  I n c l u d e  -  F i l e  ************************
 *  
 *         Name: m76_uee.h
 *
 *       Author: ls
 *        $Date: 2004/04/02 16:39:56 $
 *    $Revision: 1.2 $
 * 
 *  Description: Internal include file for M76 user eeprom 
 *                      
 *     Switches: 
 *
 *-------------------------------[ History ]---------------------------------
 *
 * $Log: m76_uee.h,v $
 * Revision 1.2  2004/04/02 16:39:56  ub
 * cosmetics  
 *
 * in between: blue screen bugfix by dp
 *
 * Revision 1.1  2001/06/05 16:24:03  Schoberl
 * Initial Revision
 *
 * Revision 1.1  2001/02/20 15:23:11  Schoberl
 * Initial Revision
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 2001 by MEN mikro elektronik GmbH, Nuernberg, Germany 
 ****************************************************************************/

#ifdef __cplusplus
      extern "C" {
#endif


#ifndef  M76_VARIANT
# define M76_VARIANT M76
#endif

# define _M76_GLOBNAME(var,name) var##_##name
#ifndef _ONE_NAMESPACE_PER_DRIVER_
# define M76_GLOBNAME(var,name) _M76_GLOBNAME(var,name)
#else
# define M76_GLOBNAME(var,name) _M76_GLOBNAME(M76,name)
#endif

#define __M76_UeeRead      M76_GLOBNAME(M76_VARIANT,UeeRead)
#define __M76_UeeWrite     M76_GLOBNAME(M76_VARIANT,UeeWrite)


/* calibration eeprom access prototypes */
extern int32 __M76_UeeRead(OSS_HANDLE *osh, MACCESS ma, u_int8 index );
extern int32 __M76_UeeWrite(OSS_HANDLE *osh, MACCESS ma, u_int8 index, u_int16 data );

#ifdef __cplusplus
      }
#endif

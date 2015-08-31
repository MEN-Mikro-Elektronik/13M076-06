/***********************  I n c l u d e  -  F i l e  ************************
 *
 *         Name: m76_drv.h
 *
 *       Author: ls
 *        $Date: 2010/03/11 17:03:14 $
 *    $Revision: 2.4 $
 *
 *  Description: Header file for M76 driver
 *               - M76 specific status codes
 *               - M76 function prototypes
 *
 *     Switches: _ONE_NAMESPACE_PER_DRIVER_
 *               _LL_DRV_
 *               M76_VARIANT
 *
 *-------------------------------[ History ]---------------------------------
 *
 * $Log: m76_drv.h,v $
 * Revision 2.4  2010/03/11 17:03:14  amorbach
 * R: Porting to MDIS5
 * M: changed according to MDIS Porting Guide 0.8
 *
 * Revision 2.3  2001/12/18 14:33:20  Schoberl
 * added: M76_ prefix to measurement range defines and CALI_VAL struct,
 * defines for variants
 *
 * Revision 2.2  2001/06/19 16:56:59  Schoberl
 * cosmetics, calibration handling status codes
 *
 * Revision 2.1  2001/06/05 16:24:23  Schoberl
 * Initial Revision
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 2010 by MEN mikro elektronik GmbH, Nuernberg, Germany
 ****************************************************************************/

#ifndef _M76_DRV_H
#define _M76_DRV_H

#ifdef __cplusplus
      extern "C" {
#endif


/*-----------------------------------------+
|  TYPEDEFS                                |
+-----------------------------------------*/
typedef struct {
	u_int32		mode;		/* calibration register (zero or full, AC/DC or R) */		
	u_int32		value;      /* value */
} M76_CALI_VAL;

/*-----------------------------------------+
|  DEFINES                                 |
+-----------------------------------------*/
/* M76 specific status codes (STD) */			/* S,G: S=setstat, G=getstat */
#define M76_RANGE       M_DEV_OF+0x00		/* G,S: measurement range */
#define M76_CALI	M_DEV_OF+0x01		/* G  :	perform a calibration */
#define M76_STORE_CALI	M_DEV_OF+0x02		/*   S:	save calibration values */
#define M76_CHECKSUM	M_DEV_OF+0x04		/* G  : cali vals checksum ok? */
#define M76_SETTLE	M_DEV_OF+0x05		/* G,S:	settling time */	
#define M76_PERMIT	M_DEV_OF+0x06		/* G,S: permit measurment with wrong checksum */
#define M76_CINFO	M_DEV_OF+0x07		/* G  : calibration info of range */
#define M76_DELMAGIC	M_DEV_OF+0x10		/*   S: delete magic word in uee */
#define M76_FILTER      M_DEV_OF+0x11		/* G,S: filter Value */
/* M76 specific status codes (BLK)	*/			/* S,G: S=setstat, G=getstat */
#define M76_BLK_CALI		M_DEV_BLK_OF+0x00 	/* S  : write a value to caliVals */


/* measurement ranges */
#define M76_RANGE_DC_V0		0	/* DC voltage, 125mV */
#define M76_RANGE_DC_V1		1	/* DC voltage, 1.25V */
#define M76_RANGE_DC_V2		2	/* DC voltage, 12.5V */
#define M76_RANGE_DC_V3		3	/* DC voltage, 125V */
#define M76_RANGE_DC_V4		4	/* DC voltage, 500V */
							
#define M76_RANGE_AC_V0		5	/* AC voltage, 250mV */
#define M76_RANGE_AC_V1		6	/* AC voltage, 2.5V */
#define M76_RANGE_AC_V2		7	/* AC voltage, 25V */
#define M76_RANGE_AC_V3		8	/* AC voltage, 250V */
							
#define M76_RANGE_DC_A0		9	/* DC current, 12.5mA */
#define M76_RANGE_DC_A1		10	/* DC current, 125mA */
#define M76_RANGE_DC_A2		11	/* DC current, 1.25A */
#define M76_RANGE_DC_A3		12	/* DC current, 2.5A */
							
#define M76_RANGE_AC_A0		13	/* AC current, 25mA */
#define M76_RANGE_AC_A1		14	/* AC current, 250mA */
#define M76_RANGE_AC_A2		15	/* AC current, 2.5A */
		  
#define M76_RANGE_R2_0		16	/* resistance, 2-wire, 250 OHM  */
#define M76_RANGE_R2_1		17	/* resistance, 2-wire, 2.5 kOHM */
#define M76_RANGE_R2_2		18	/* resistance, 2-wire, 25 kOHM  */
#define M76_RANGE_R2_3		19	/* resistance, 2-wire, 250 kOHM */
#define M76_RANGE_R2_4		20	/* resistance, 2-wire, 2.5 MOHM */

#define M76_RANGE_R4_0		21	/* resistance, 4-wire, 250 OHM  */
#define M76_RANGE_R4_1		22	/* resistance, 4-wire, 2.5 kOHM */
#define M76_RANGE_R4_2		23	/* resistance, 4-wire, 25 kOHM  */
#define M76_RANGE_R4_3		24	/* resistance, 4-wire, 250 kOHM */
#define M76_RANGE_R4_4		25	/* resistance, 4-wire, 2.5 MOHM */


#ifndef  M76_VARIANT
# define M76_VARIANT M76
#endif

# define _M76_GLOBNAME(var,name) var##_##name
#ifndef _ONE_NAMESPACE_PER_DRIVER_
# define M76_GLOBNAME(var,name) _M76_GLOBNAME(var,name)
#else
# define M76_GLOBNAME(var,name) _M76_GLOBNAME(M76,name)
#endif

#define __M76_GetEntry			M76_GLOBNAME(M76_VARIANT,GetEntry)


/*-----------------------------------------+
|  PROTOTYPES                              |
+-----------------------------------------*/
#ifdef _LL_DRV_
#ifndef _ONE_NAMESPACE_PER_DRIVER_
	extern void __M76_GetEntry(LL_ENTRY* drvP);
#endif
#endif /* _LL_DRV_ */

/*-----------------------------------------+
|  BACKWARD COMPATIBILITY TO MDIS4         |
+-----------------------------------------*/
#ifndef U_INT32_OR_64
    /* we have an MDIS4 men_types.h and mdis_api.h included */
    /* only 32bit compatibility needed!                     */
    #define INT32_OR_64  int32
        #define U_INT32_OR_64 u_int32
    typedef INT32_OR_64  MDIS_PATH;
#endif /* U_INT32_OR_64 */


#ifdef __cplusplus
      }
#endif

#endif /* _M76_DRV_H */





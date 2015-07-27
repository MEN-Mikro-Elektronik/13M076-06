/*********************  P r o g r a m  -  M o d u l e ***********************
 *  
 *         Name: m76_uee.c
 *      Project: M76 ll-driver
 *
 *       Author: ls
 *        $Date: 2004/04/02 16:39:53 $
 *    $Revision: 1.3 $
 *
 *  Description: M76 user eeprom access (cloned/adapted from 
 *                                       c_drvadd.c/microwire_port.c)
 *                      
 *                      
 *     Required:  
 *     Switches:  
 *
 *---------------------------[ Public Functions ]----------------------------
 *  M76_UeeRead, M76_UeeWrite
 *  
 *-------------------------------[ History ]---------------------------------
 *
 * $Log: m76_uee.c,v $
 * Revision 1.3  2004/04/02 16:39:53  ub
 * cosmetics
 *
 * Revision 1.2  2001/06/19 16:56:48  Schoberl
 * cosmetics
 *
 * Revision 1.1  2001/06/05 16:24:01  Schoberl
 * Initial Revision
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 2001 by MEN Mikro Elektronik GmbH, Nuernberg, Germany 
 ****************************************************************************/

#include <MEN/men_typs.h>           
#include <MEN/dbg.h>
#include <MEN/oss.h>
#include <MEN/maccess.h>

#include "m76_uee.h"

/*--- instructions for serial EEPROM ---*/
#define     OPSH    2
#define     OPLEN   10

#define     _READ_  (0x80<<OPSH)    /* read data */
#define     EWEN    (0x30<<OPSH)    /* enable erase/write state */
#define     ERASE   (0xc0<<OPSH)    /* erase cell */
#define     _WRITE_ (0x40<<OPSH)    /* write data */
#define     EWDS    (0x00<<OPSH)    /* disable erase/write state */

#define     T_WP    10000   /* max. time required for write/erase (us) */

/* bit definition */
#define B_DAT   0x01                /* data in-;output      */
#define B_CLK   0x02                /* clock                */
#define B_SEL   0x04                /* chip-select          */

/* A08 register address */
#define     MODREG  0xfe

/*--- K&R prototypes ---*/
static int32 _write( OSS_HANDLE *osh, u_int32 base, u_int8 index, u_int16 data );
static int32 _erase( OSS_HANDLE *osh, u_int32 base, u_int8 index );
static void _opcode( OSS_HANDLE *osh, u_int32 base, u_int16 code );
static void _select( OSS_HANDLE *osh, u_int32 base );
static void _deselect( u_int32 base );
static int16 _clock( OSS_HANDLE *osh, u_int32 base, u_int8 dbs );
static void _delay( OSS_HANDLE *osh );

/******************************* M76_UeeWrite *******************************
 *
 *  Description:  Write a specified word into EEPROM at 'base'.
 *
 *---------------------------------------------------------------------------
 *  Input......:  osh      OSS_HANDLE
 *                addr     base address pointer
 *                index    index to write (0..15)
 *                data     word to write
 *  Output.....:  return   0..ok | error
 *  Globals....:  ---
 ***************************************************************************/
extern int32 __M76_UeeWrite                 /* nodoc */
(OSS_HANDLE *osh, u_int8 *addr, u_int8  index, u_int16 data )
{
    if( _erase(osh, (u_int32)addr, index ))              /* erase cell first */
        return 3;

    return _write(osh, (u_int32)addr, index, data );
}

/******************************* M76_UeeRead ********************************
 *
 *  Description:  Read a specified word from EEPROM at 'base'.
 *
 *---------------------------------------------------------------------------
 *  Input......:  addr     base address pointer
 *                index    index to read (0..15)
 *  Output.....:  return   readed word
 *  Globals....:  ---
 ****************************************************************************/
extern int32 __M76_UeeRead(OSS_HANDLE *osh, u_int32 base, u_int8 index ) /* nodoc */
{
    register u_int16    wx;                 /* data word    */
    register int32       i;                 /* counter      */

    _opcode(osh, base, (_READ_+index) );
    for(wx=0, i=0; i<16; i++)
        wx = (wx<<1)+_clock(osh,base,0);
    _deselect(base);

    return(wx);
}


/******************************* _write ************************************
 *
 *  Description:  Write a specified word into EEPROM at 'base'.
 *
 *---------------------------------------------------------------------------
 *  Input......:  index    index to write (0..255)
 *                data     word to write
 *  Output.....:  return   0=ok 1=write err 2=verify err
 *  Globals....:  ---
 ***************************************************************************/
static int32 _write                                         /* nodoc */
(OSS_HANDLE *osh, u_int32 base, u_int8 index, u_int16 data ) 
{
    register int    i,j;                    /* counters     */

    _opcode(osh,base,EWEN);                     /* write enable */
    _deselect(base);                        /* deselect     */

    _opcode(osh,base, (_WRITE_+index) );             /* select write */
    for(i=15; i>=0; i--)
        _clock(osh,base,(u_int8)((data>>i)&0x01));        /* write data   */
    _deselect(base);                        /* deselect     */

    _select(osh,base);
    for(i=T_WP; i>0; i--)                   /* wait for low */
    {   if(!_clock(osh,base,0))
            break;
        _delay(osh);
    }
    for(j=T_WP; j>0; j--)                   /* wait for high*/
    {   if(_clock(osh,base,0))
            break;
        _delay(osh);
    }

    _opcode(osh,base, EWDS);                /* write disable*/
    _deselect(base);                        /* disable      */

    if((i==0) || (j==0))                    /* error ?      */
        return 1;                           /* ..yes */

    if( data != __M76_UeeRead(osh, base,index) ) /* verify data  */
        return 2;                           /* ..error      */

    return 0;                               /* ..no         */
}


/******************************* _erase ************************************
 *
 *  Description:  Erase a specified word into EEPROM
 *
 *---------------------------------------------------------------------------
 *  Input......:  index    index to write (0..15)
 *  Output.....:  return   0=ok 1=error
 *  Globals....:  ---
 ***************************************************************************/
static int32 _erase( OSS_HANDLE *osh, u_int32 base, u_int8 index )  /* nodoc */
{
    register int    i,j;                    /* counters     */

    _opcode(osh, base,EWEN);                /* erase enable */
    _deselect(base);                        /* deselect     */

    _opcode(osh,base,(ERASE+index) );        /* select erase */
    _deselect(base);                        /* deselect     */

    _select(osh,base);
    for(i=T_WP; i>0; i--)                   /* wait for low */
    {   if(!_clock(osh,base,0))
            break;
        _delay(osh);
    }

    for(j=T_WP; j>0; j--)                   /* wait for high*/
    {   if(_clock(osh,base,0))
            break;
        _delay(osh);
    }

    _opcode(osh,base,EWDS);                     /* erase disable*/
    _deselect(base);                        /* disable      */

    if((i==0) || (j==0))                    /* error ?      */
        return 1;

    return 0;
}


/******************************* _opcode ************************************
 *
 *  Description:  Output opcode with leading startbit
 *
 *---------------------------------------------------------------------------
 *  Input......:  code     opcode to write
 *  Output.....:  -
 *  Globals....:  ---
 ***************************************************************************/
static void _opcode(OSS_HANDLE *osh, u_int32 base, u_int16 code ) /* nodoc */
{
    register int i;

    _select(osh,base);
    _clock(osh,base,1);                         /* output start bit */

    for(i=OPLEN-1; i>=0; i--)
        _clock(osh,base,(u_int8)((code>>i)&0x01) );/* output instruction code  */
}



/*----------------------------------------------------------------------
 * LOW-LEVEL ROUTINES FOR SERIAL EEPROM
 *--------------------------------------------------------------------*/

/******************************* _select ************************************
 *
 *  Description:  Select EEPROM:
 *                 output DI/CLK/CS low
 *                 delay
 *                 output CS high
 *                 delay
 *---------------------------------------------------------------------------
 *  Input......:  -
 *  Output.....:  -
 *  Globals....:  ---
 ***************************************************************************/
static void _select(OSS_HANDLE *osh, u_int32 base ) /* nodoc */
{
    MWRITE_D16( base, MODREG, 0 );          /* everything inactive */
    _delay(osh);
    MWRITE_D16( base, MODREG, B_SEL );      /* select high */
    _delay(osh);
}

/******************************* _deselect **********************************
 *
 *  Description:  Deselect EEPROM
 *                 output CS low
 *---------------------------------------------------------------------------
 *  Input......:  -
 *  Output.....:  -
 *  Globals....:  ---
 ***************************************************************************/
static void _deselect( u_int32 base ) /* nodoc */
{
    MWRITE_D16( base, MODREG, 0 );          /* everything inactive */
}


/******************************* _clock ************************************
 *
 *  Description:  Output data bit:
 *                 output clock low
 *                 output data bit
 *                 delay
 *                 output clock high
 *                 delay
 *                 return state of data serial eeprom's DO - line
 *                 (Note: keep CS asserted)
 *---------------------------------------------------------------------------
 *  Input......:  data bit to send
 *  Output.....:  state of DO line
 *  Globals....:  ---
 ***************************************************************************/
static int16 _clock(OSS_HANDLE *osh, u_int32 base, u_int8 dbs ) /* nodoc */
{
    MWRITE_D16( base, MODREG, dbs|B_SEL );  /* output clock low */
                                            /* output data high/low */
    _delay(osh);                               /* delay    */

    MWRITE_D16( base, MODREG, dbs|B_CLK|B_SEL );  /* output clock high */
    _delay(osh);                               /* delay    */

    return( MREAD_D16( base, MODREG) & B_DAT );  /* get data */
}

/******************************* _delay ******************************
 *
 *  Description:  Delay 
 *---------------------------------------------------------------------------
 *  Input......:  -
 *  Output.....:  -
 *  Globals....:  ---
 ***************************************************************************/
static void _delay( OSS_HANDLE *osh ) /* nodoc */
{

    OSS_MikroDelay( osh, 100 );

}


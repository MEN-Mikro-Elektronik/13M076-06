/*********************  P r o g r a m  -  M o d u l e ***********************
 *
 *         Name: m76_drv.c
 *      Project: M76 module driver (MDIS5)
 *
 *       Author: ls
 *        $Date: 2010/03/11 17:04:34 $
 *    $Revision: 1.8 $
 *
 *  Description: Low-level driver for M76 modules
 *
 *               The M76 module is a digital system multimeter M-Module for
 *               measuring DC and AC voltages, DC and AC currents, and
 *               resistance by means of 2-wire and/or 4-wire connect.
 *
 *
 *     Required: OSS, DESC, DBG, ID libraries 
 *     Switches: _ONE_NAMESPACE_PER_DRIVER_
 *
 *-------------------------------[ History ]---------------------------------
 *
 * $Log: m76_drv.c,v $
 * Revision 1.8  2010/03/11 17:04:34  amorbach
 * R: Porting to MDIS5
 * M: changed according to MDIS Porting Guide 0.8
 *
 * Revision 1.7  2004/04/02 16:39:51  ub
 * added some typecasts
 * cosmetics
 *
 * Revision 1.6  2002/08/27 11:47:30  LSchoberl
 * defines for configuration register settings for meas. ranges changed 
 *
 * Revision 1.5  2001/12/18 14:33:02  Schoberl
 * define for AC_V3 changed, M76_Irq returns LL_IRQ_DEVICE if irq was caused
 * by device, cosmetics
 *
 * Revision 1.4  2001/06/26 09:21:01  Schoberl
 * cosmetics
 *
 * Revision 1.3  2001/06/19 16:56:42  Schoberl
 * check magic word in user EEPROM, check calibration of range, user EEPROM
 * handling, cosmetics
 *
 * Revision 1.2  2001/06/08 10:20:20  kp
 * fixed endianess problems in ReadCaliProm and WriteCaliProm
 *
 * Revision 1.1  2001/06/05 16:23:58  Schoberl
 * Initial Revision
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 2010 by MEN Mikro Elektronik GmbH, Nuremberg, Germany
 ****************************************************************************/

static const char RCSid[]="$Id: m76_drv.c,v 1.8 2010/03/11 17:04:34 amorbach Exp $";

#define _NO_LL_HANDLE       /* ll_defs.h: don't define LL_HANDLE struct */

#ifdef MAC_BYTESWAP         /* swapped variant */
#   define ID_SW    
#endif

#include <MEN/men_typs.h>   /* system dependent definitions   */
#include <MEN/maccess.h>    /* hw access macros and types     */
#include <MEN/dbg.h>        /* debug functions                */
#include <MEN/oss.h>        /* oss functions                  */
#include <MEN/desc.h>       /* descriptor functions           */
#include <MEN/mdis_api.h>   /* MDIS global defs               */
#include <MEN/mdis_com.h>   /* MDIS common defs               */
#include <MEN/mdis_err.h>   /* MDIS error codes               */
#include <MEN/ll_defs.h>    /* low-level driver definitions   */
#include <MEN/microwire.h>  /* ID PROM functions              */
#include "m76_uee.h"        /* user EEPROM functions          */

/*-----------------------------------------+
|  DEFINES                                 |
+-----------------------------------------*/
/* general */
#define CH_NUMBER           1           /* number of device channels */
#define USE_IRQ             TRUE        /* interrupt required  */
#define ADDRSPACE_COUNT     1           /* nr of required address spaces */
#define ADDRSPACE_SIZE      256         /* size of address space */
#define MOD_ID_MAGIC        0x5346      /* ID PROM magic word */
#define MOD_ID_SIZE         128         /* ID PROM size [bytes] */
#define MOD_ID              76          /* ID PROM module ID */
#define UEE_MAGIC           0x3730      /* user EEPROM magic word */
#define UEE_MAGIC_ADDRESS   0x91        /* address for user EEPROM magic word */
#define POLL_TOUT           200         /* timeout for polled read access */

/* debug settings */
#define DBG_MYLEVEL         llHdl->dbgLevel 
#define DBH                 llHdl->dbgHdl

/* module register offsets */
#define DATA_REG            0x00        /* base of Data Register */
#define CONFIG_REG          0x04        /* base of Configuration Register */
#define COM_REG             0x08        /* base of Communication Register */
#define ACCESS_REG          0x0a        /* base of Access Register */
#define STAT_REG            0x0c        /* base of Status Register */
#define CTRL_REG            0x0e        /* base of Control Register */
#define PLD_IF_REG          0xfe        /* register offset to access ID PROM */

/* bits in access register 0x0a */
#define IRQ         0x8
#define TR24R       0x4         /* 24-bit read transfer after DRDY */

/* bits in status register 0x0c */
#define IRQ_PEND    0x8         /* irq pending */
#define TRDYR       0x4         /* transfer ready after TR24R */            

/* bits in control register 0x0e */
#define IDPROM_SEL  0x2         /* select ID PROM & clr reset state */
#define UEPROM_SEL  0x6         /* select user EEPROM */


/* ADC registers */
/* ADC's Communication Register */
/* defines for RS0 RS1 RS2 in ADC's communication register */
#define COM_COM         (0<<4)          /*0x00 Communication Register */
#define COM_MODE        (1<<4)          /*0x10 Mode Register */
#define COM_FILTER_HIGH (2<<4)          /*0x20 Filter High Register */
#define COM_FILTER_LOW  (3<<4)          /*0x30 Filter Low Register */
#define COM_DATA        (5<<4)          /*0x50 Data Register */
#define COM_CALI_ZERO   (6<<4)          /*0x60 Zero-Scale Calibration Register */
#define COM_CALI_FULL   (7<<4)          /*0x70 Full-Scale Calibration Register */
/* read write select */
#define COM_READ        (1<<3)          /* next op is a read */
/* channel select */
#define COM_R_I     0x2             /* current of resistance measurement */
#define COM_R_U     0x3             /* voltage of resistance measurement */
#define COM_DC      0x4             /* DC voltage/current measurement */
#define COM_AC      0x6             /* AC voltage/current measurement */

/* ADC's Mode Register (access via module's Data Register) */
/* measurement mode (MD0, MD1, MD2)*/
#define MOD_NORMAL      (0<<5)          /* normal measurement mode */
#define MOD_ZERO        (2<<5)          /* zero-scale system calibration */
#define MOD_FULL        (3<<5)          /* full-scale system calibration */
/* gain (G0, G1, G2) */
#define MOD_GAIN_1      (0<<2)          /* gain = 1 */
#define MOD_GAIN_4      (2<<2)          /* gain = 4 */
                        
/* ADC's Filter High Register */
#define FHI_POLAR_BI    (0<<7)          /* bipolar measurement */
#define FHI_POLAR_UNI   (1<<7)          /* unipolar measurement */
#define FHI_WL          (1<<6)          /* 24-bit word length */

/* measurement ranges (Configuration Register) */
#define DC_V0   0x06fae600  /* DC voltage range 125mV */
#define DC_V1   0x06fae500  /* DC voltage range 1.25V */
#define DC_V2   0x02fae600  /* DC voltage range 12.5V */
#define DC_V3   0x02fae500  /* DC voltage range 125V and 500V */
#define DC_V4   0x02fae500  /* DC voltage range 125V and 500V */

#define AC_V0   0x04fbf500  /* AC voltage range 250mV */
#define AC_V1   0x04fbe600  /* AC voltage range 2.5V */
#define AC_V2   0x04fbd600  /* AC voltage range 25V */
#define AC_V3   0x00fbb600  /* AC voltage range 250V */

#define DC_A0   0x287de500  /* DC current range 12.5mA */
#define DC_A1   0x28bde500  /* DC current range 125mA */
#define DC_A2   0x30dde500  /* DC current range 1.25A */
#define DC_A3   0x30ede500  /* DC current range 2.5A */

#define AC_A0   0x0877e500  /* AC current range 25mA */
#define AC_A1   0x08b7e500  /* AC current range 250mA */
#define AC_A2   0x10d7e500  /* AC current range 2.5A */
          
#define R2_0    0x4bf9bd00  /* resistance, 2-wire, 250 Ohm   */
#define R2_1    0x4bf9bd00  /* resistance, 2-wire, 2,5 kOhm  */
#define R2_2    0x4bf9dd00  /* resistance, 2-wire, 25 kOhm */
#define R2_3    0x4bf9ed00  /* resistance, 2-wire, 250 kOhm   */
#define R2_4    0x4bf9f500  /* resistance, 2-wire, 2,5 MOhm  */

#define R4_0    0x43f9bd00  /* resistance, 4-wire, 250 Ohm */
#define R4_1    0x43f9bd00  /* resistance, 4-wire, 2,5 kOhm   */
#define R4_2    0x43f9dd00  /* resistance, 4-wire, 25 kOhm  */
#define R4_3    0x43f9ed00  /* resistance, 4-wire, 250 kOhm */
#define R4_4    0x43f9f500  /* resistance, 4-wire, 2,5 MOhm   */

/* ... */

/*-----------------------------------------+
|  TYPEDEFS                                |
+-----------------------------------------*/
/* user EEPROM */

typedef struct  {   /* voltage/current */
    int32       zero;       /* zero-scale calibration value */
    int32       full;       /* full-scale calibration value */
} CALI_VA;

typedef struct  {   /* resistor */
    CALI_VA     rShort;     /* shorted in */
    CALI_VA     rOpen;      /* open in */
} CALI_R;

typedef struct  {
    CALI_VA     dcV[5];
    CALI_VA     acV[4];
    CALI_VA     dcA[4];
    CALI_VA     acA[3];
    CALI_R      r2[5];
    CALI_R      r4[5];
} CALI_VALS;

#define CALI_SIZE       sizeof(CALI_VALS)



/* low-level handle */
typedef struct {
    /* general */
    int32           memAlloc;       /* size allocated for the handle */
    OSS_HANDLE      *osHdl;         /* oss handle */
    OSS_IRQ_HANDLE  *irqHdl;        /* irq handle */
    DESC_HANDLE     *descHdl;       /* desc handle */
    MACCESS         ma;             /* hw access handle */
    MDIS_IDENT_FUNCT_TBL idFuncTbl; /* id function table */
    /* debug */
    u_int32         dbgLevel;       /* debug level */
    DBG_HANDLE      *dbgHdl;        /* debug handle */
    /* misc */
    u_int32         irqCount;       /* interrupt counter */
    u_int32         idCheck;        /* id check enabled */
    OSS_SEM_HANDLE  *readSem;       /* data ready */
    /* module */
    u_int32         irqEnable;      /* irq allowed */
    u_int32         conMode;        /* measuring mode (config reg) */
    u_int16         comChan;        /* ADC channel selection */
    u_int16         modGain;        /* gain */
    u_int16         modMode;        /* operation mode (normal/calibration) */
    u_int16         filFilter;      /* filter */
    u_int16         filPolarity;    /* uni/bipolar operation */
    u_int32         range;          /* range */
    u_int32         checkSum;       /* result of checksum check */
    u_int32         permitMeas;     /* allow Measurements even if wrong checksum */
    u_int32         calibOk;        /* actual range calibrated */
    u_int32         settleTime;     /* settle time after changing range/ADC channel */
    CALI_VALS       caliVals;       /* calibration memory */

    MCRW_HANDLE    *mcrwHdl;        /* microwire handle for IDPROM */
} LL_HANDLE;

/* include files which need LL_HANDLE */
#include <MEN/ll_entry.h>   /* low-level driver branch table  */
#include <MEN/m76_drv.h>    /* M76 driver header file */

/*-----------------------------------------+
|  PROTOTYPES                              |
+-----------------------------------------*/
static int32 M76_Init(DESC_SPEC *descSpec, OSS_HANDLE *osHdl,
                       MACCESS *ma, OSS_SEM_HANDLE *devSemHdl,
                       OSS_IRQ_HANDLE *irqHdl, LL_HANDLE **llHdlP);
static int32 M76_Exit(LL_HANDLE **llHdlP );
static int32 M76_Read(LL_HANDLE *llHdl, int32 ch, int32 *value);
static int32 M76_Write(LL_HANDLE *llHdl, int32 ch, int32 value);
static int32 M76_SetStat(LL_HANDLE *llHdl,int32 ch, int32 code, INT32_OR_64 value32_or_64);
static int32 M76_GetStat(LL_HANDLE *llHdl, int32 ch, int32 code, INT32_OR_64 *value32_or_64P);
static int32 M76_BlockRead(LL_HANDLE *llHdl, int32 ch, void *buf, int32 size,
                            int32 *nbrRdBytesP);
static int32 M76_BlockWrite(LL_HANDLE *llHdl, int32 ch, void *buf, int32 size,
                             int32 *nbrWrBytesP);
static int32 M76_Irq(LL_HANDLE *llHdl );
static int32 M76_Info(int32 infoType, ... );

static char* Ident( void );
static int32 Cleanup(LL_HANDLE *llHdl, int32 retCode);

static int32 MakeMwHandle( LL_HANDLE *llHdl );

static int32 ReadCaliProm(LL_HANDLE *llHdl);
static int32 WriteCaliProm(LL_HANDLE *llHdl);
static int32 WriteCaliReg(LL_HANDLE *llHdl);
static int32 WriteCaliRegUpd(LL_HANDLE *llHdl);
static int32 CalibAdc(LL_HANDLE *llHandle, int32 *value);
static int32 WriteConfigReg(LL_HANDLE *llHdl);
static int32 WriteModeReg(LL_HANDLE *llHdl);
static int32 WriteFilterReg(LL_HANDLE *llHdl);
static int32 ReadDataReg(LL_HANDLE *llHdl, int32 *value, int32 reg);
static int32 WriteCaliVal(LL_HANDLE *llHdl, u_int32 mode, u_int32 val);
static int32 UeeWrite(LL_HANDLE *llHdl, u_int8 index, u_int16 value);

/**************************** M76_GetEntry *********************************
 *
 *  Description:  Initialize driver's branch table
 *
 *---------------------------------------------------------------------------
 *  Input......:  ---
 *  Output.....:  drvP  pointer to the initialized branch table structure
 *  Globals....:  ---
 ****************************************************************************/
#ifdef _ONE_NAMESPACE_PER_DRIVER_
    extern void LL_GetEntry( LL_ENTRY* drvP )
#else
    extern void __M76_GetEntry( LL_ENTRY* drvP )
#endif
{
    drvP->init        = M76_Init;
    drvP->exit        = M76_Exit;
    drvP->read        = M76_Read;
    drvP->write       = M76_Write;
    drvP->blockRead   = M76_BlockRead;
    drvP->blockWrite  = M76_BlockWrite;
    drvP->setStat     = M76_SetStat;
    drvP->getStat     = M76_GetStat;
    drvP->irq         = M76_Irq;
    drvP->info        = M76_Info;
}

/******************************** M76_Init ***********************************
 *
 *  Description:  Allocate and return low-level handle, initialize hardware
 * 
 *                The function makes following settings:
 *                - range: M76_RANGE_AC_V3
 *                - filter: 1920 (10Hz)
 *                - settling time: 700 ms
 *
 *                The following descriptor keys are used:
 *
 *                Descriptor key        Default          Range
 *                --------------------  ---------------  -------------
 *                DEBUG_LEVEL_DESC      OSS_DBG_DEFAULT  see dbg.h
 *                DEBUG_LEVEL           OSS_DBG_DEFAULT  see dbg.h
 *                ID_CHECK              1                0..1
 *---------------------------------------------------------------------------
 *  Input......:  descSpec   pointer to descriptor data
 *                osHdl      oss handle
 *                ma         hardware access handle
 *                devSemHdl  device semaphore handle
 *                irqHdl     irq handle
 *  Output.....:  llHdlP     pointer to low-level driver handle
 *                return     success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 M76_Init(
    DESC_SPEC       *descP,
    OSS_HANDLE      *osHdl,
    MACCESS         *ma,
    OSS_SEM_HANDLE  *devSemHdl,
    OSS_IRQ_HANDLE  *irqHdl,
    LL_HANDLE       **llHdlP
)
{
    LL_HANDLE *llHdl = NULL;
    u_int32 gotsize;
    int32 error;
    u_int32 value;

    /*------------------------------+
    |  prepare the handle           |
    +------------------------------*/
    *llHdlP = NULL;     /* set low-level driver handle to NULL */ 
    
    /* alloc */
    if ((llHdl = (LL_HANDLE*)OSS_MemGet(
                    osHdl, sizeof(LL_HANDLE), &gotsize)) == NULL)
       return(ERR_OSS_MEM_ALLOC);

    /* clear */
    OSS_MemFill(osHdl, gotsize, (char*)llHdl, 0x00);

    /* init */
    llHdl->memAlloc   = gotsize;
    llHdl->osHdl      = osHdl;
    llHdl->irqHdl     = irqHdl;
    llHdl->ma         = *ma;

    /*--- create handle for Microwire lib ---*/
    if( (error = MakeMwHandle( llHdl )))
        return( Cleanup(llHdl,error) );

    /*------------------------------+
    |  init id function table       |
    +------------------------------*/
    /* driver's ident function */
    llHdl->idFuncTbl.idCall[0].identCall = Ident;
    /* library's ident functions */
    llHdl->idFuncTbl.idCall[1].identCall = DESC_Ident;
    llHdl->idFuncTbl.idCall[2].identCall = OSS_Ident;
    llHdl->idFuncTbl.idCall[3].identCall = llHdl->mcrwHdl->Ident;
    /* terminator */
    llHdl->idFuncTbl.idCall[4].identCall = NULL;

    /*------------------------------+
    |  prepare debugging            |
    +------------------------------*/
    DBG_MYLEVEL = OSS_DBG_DEFAULT;  /* set OS specific debug level */
    DBGINIT((NULL,&DBH));

    /*------------------------------+
    |  scan descriptor              |
    +------------------------------*/
    /* prepare access */
    if ((error = DESC_Init(descP, osHdl, &llHdl->descHdl)))
        return( Cleanup(llHdl,error) );

    /* DEBUG_LEVEL_DESC */
    if ((error = DESC_GetUInt32(llHdl->descHdl, OSS_DBG_DEFAULT, 
                                &value, "DEBUG_LEVEL_DESC")) &&
        error != ERR_DESC_KEY_NOTFOUND)
        return( Cleanup(llHdl,error) );

    DESC_DbgLevelSet(llHdl->descHdl, value);    /* set level */

    /* DEBUG_LEVEL */
    if ((error = DESC_GetUInt32(llHdl->descHdl, OSS_DBG_DEFAULT, 
                                &llHdl->dbgLevel, "DEBUG_LEVEL")) &&
        error != ERR_DESC_KEY_NOTFOUND)
        return( Cleanup(llHdl,error) );

    DBGWRT_1((DBH, "LL - M76_Init\n"));

    /* ID_CHECK */
    if ((error = DESC_GetUInt32(llHdl->descHdl, TRUE, 
                                &llHdl->idCheck, "ID_CHECK")) &&
        error != ERR_DESC_KEY_NOTFOUND)
        return( Cleanup(llHdl,error) );

    /*------------------------------+
    |  clr reset                    |
    +------------------------------*/
    MWRITE_D16(llHdl->ma, CTRL_REG, IDPROM_SEL);

    /*------------------------------+
    |  check module ID              |
    +------------------------------*/
    if (llHdl->idCheck) {
        struct { u_int16 modIdMagic, modId; } id;

        error = llHdl->mcrwHdl->ReadEeprom( (void *)llHdl->mcrwHdl,
                                            0, (u_int16 *)&id, 
                                            sizeof(id));
        if( error ){
            DBGWRT_ERR((DBH," *** M76_Init: error 0x%x reading EEPROM\n",
                        error));
            error = ERR_LL_ILL_ID;
            return( Cleanup(llHdl,error) );
        }
        if (id.modIdMagic != MOD_ID_MAGIC) {
            DBGWRT_ERR((DBH," *** M76_Init: illegal magic=0x%04x\n",
                        id.modIdMagic));
            error = ERR_LL_ILL_ID;
            return( Cleanup(llHdl,error) );
        }
        if (id.modId != MOD_ID) {
            DBGWRT_ERR((DBH," *** M76_Init: illegal id=%d\n",id.modId));
            error = ERR_LL_ILL_ID;
            return( Cleanup(llHdl,error) );
        }
    }
    
    /*------------------------------+
    |  set up module variables      |
    +------------------------------*/
    /*--- read semaphore ---*/
    if( (error = OSS_SemCreate( llHdl->osHdl, OSS_SEM_BIN, 0, &llHdl->readSem )))
        return( Cleanup(llHdl,error) );

    /*------------------------------+
    |  init hardware                |
    +------------------------------*/
    /* check magic word of user EEPROM */
    /* if there is no magic word in user EEPROM then set all cells used for */
    /* calibration values to 0xffff */
    MWRITE_D16(llHdl->ma, CTRL_REG, UEPROM_SEL);    /* select user EEPROM */
    if ( __M76_UeeRead(llHdl->osHdl, llHdl->ma,
             (u_int8)UEE_MAGIC_ADDRESS) != UEE_MAGIC )  
    {
        u_int16 idx;
    
        DBGWRT_2((DBH, " No UEE_MAGIC -> Set all uee calibration cells to 0xffff\n"));

        for (idx=0; idx < (sizeof(llHdl->caliVals)/2+1); idx++)  {
            error = UeeWrite(llHdl,(u_int8)idx, 0xffff);
            if (error)  {
                break;
            }
        }
        /* write MAGIC */
        if (error == 0)  {
            error = UeeWrite(llHdl,(u_int8)UEE_MAGIC_ADDRESS,(u_int16)UEE_MAGIC);
        }
    }   
    MWRITE_D16(llHdl->ma, CTRL_REG, IDPROM_SEL);    /* deselect user EEPROM */
    if (error)      /* error deleting uee */
        return( Cleanup(llHdl,error) );
    
    /* read calibration EEPROM */
    if ( (error = ReadCaliProm(llHdl)) )  {
        /* invalid checksum */
        llHdl->checkSum = FALSE;    
        llHdl->permitMeas = FALSE;
    }
    else  {
        /* valid checksum */
        llHdl->checkSum = TRUE;
        llHdl->permitMeas = TRUE;
    }
    
    /* default */
    llHdl->range = M76_RANGE_AC_V3;
    llHdl->conMode = AC_V3; 
    llHdl->comChan = COM_AC;
    llHdl->modGain = MOD_GAIN_1;
    llHdl->modMode = MOD_NORMAL;
    llHdl->filFilter =  1920;           /* 10Hz */
    llHdl->filPolarity = FHI_POLAR_UNI;
    llHdl->settleTime = 700;            

    WriteConfigReg(llHdl);
    WriteFilterReg(llHdl);
    WriteModeReg(llHdl);
    WriteCaliRegUpd(llHdl);/* write new cali val to ADC,*/
                           /*  update cali info */
    OSS_Delay(llHdl->osHdl, llHdl->settleTime);

    *llHdlP = llHdl;    /* set low-level driver handle */

    return(ERR_SUCCESS);
}

/****************************** M76_Exit *************************************
 *
 *  Description:  De-initialize hardware and clean up memory
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdlP    pointer to low-level driver handle
 *  Output.....:  return    success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 M76_Exit(
   LL_HANDLE    **llHdlP
)
{
    LL_HANDLE *llHdl = *llHdlP;
    int32 error = 0;

    DBGWRT_1((DBH, "LL - M76_Exit\n"));

    /*------------------------------+
    |  de-init hardware             |
    +------------------------------*/
    MWRITE_D16(llHdl->ma, ACCESS_REG, 0);   
    
    /*------------------------------+
    |  clean up memory               |
    +------------------------------*/
    *llHdlP = NULL;     /* set low-level driver handle to NULL */ 
    error = Cleanup(llHdl,error);

    return(error);
}

/****************************** M76_Read *************************************
 *
 *  Description:  Read a value from the device, to be used for DC/AC current
 *                and voltage measurements.
 *                
 *                Reads a 24-bit value from the A/D converter. The bits are
 *                right-aligned in a 32-bit long word. The upper 8 bits are
 *                always zero.
 *
 *                If the checksum test result is FALSE, an error is returned 
 *                as long as read is not explicitly allowed.
 *                If there are no valid calibration values for the selected 
 *                range an error is returned.
 *---------------------------------------------------------------------------
 *  Input......:  llHdl    low-level handle
 *                ch       current channel
 *  Output.....:  valueP   read value
 *                return   success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 M76_Read(
    LL_HANDLE *llHdl,
    int32 ch,
    int32 *valueP
)
{
    int32 error=ERR_SUCCESS;

    DBGWRT_1((DBH, "LL - M76_Read: ch=%d\n",ch));

    if (llHdl->range > M76_RANGE_AC_A2)     /* wrong range */
        return(ERR_LL_ILL_PARAM);
    if (llHdl->permitMeas == FALSE)     /* wrong checksum */
        return(ERR_LL_DEV_NOTRDY);
    if (llHdl->calibOk == FALSE)        /* not calibrated */
        return(ERR_LL_DEV_NOTRDY);

    error = ReadDataReg(llHdl, valueP, COM_DATA);

    *valueP >>= 8;
    *valueP &= 0x00ffffff;

    return (error);

}

/****************************** M76_Write ************************************
 *
 *  Description:  Function not supported on M76
 *---------------------------------------------------------------------------
 *  Input......:  llHdl    low-level handle
 *                ch       current channel
 *                value    value to write 
 *  Output.....:  return   success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 M76_Write(
    LL_HANDLE *llHdl,
    int32 ch,
    int32 value
)
{
    DBGWRT_1((DBH, "LL - M76_Write: ch=%d\n",ch));
    
    return(ERR_LL_ILL_FUNC);
}

/****************************** M76_SetStat **********************************
 *
 *  Description:  Set the driver status
 *
 *                The following status codes are supported:
 *
 *                Code                 Description                 Values
 *                -------------------  --------------------------  ----------
 *                M_LL_DEBUG_LEVEL     driver debug level          see dbg.h
 *                M_MK_IRQ_ENABLE      interrupt enable            0..1
 *                M_LL_IRQ_COUNT       interrupt counter           0..max
 *                M_LL_CH_DIR          direction of curr. chan.    M_CH_IN
 *                M76_RANGE            select measurement range    M76_RANGE_xxx
 *                M76_SETTLE           settling time               0..max ms
 *                M76_PERMIT           permit measurement if       0..1
 *                                     checksum is wrong    
 *                M76_STORE_CALI    *) store calibration memory    UEE_MAGIC 
 *                                     in user EEPROM
 *                M76_BLK_CALI      *) write a value to            see below
 *                                     calibration memory
 *                M76_DELMAGIC      *) delete magic word in        UEE_MAGIC
 *                                     user EEPROM
 *
 *                !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
 *                !*) Note:  MEN carries out an initial calibration. The    !
 *                !          values of this procedure are stored in the     !
 *                !          user EEPROM at delivery of the M76 M-Module.   !
 *                !                                                         !  
 *                !!   DO NOT ALTER THE STORED CALIBRATION VALUES OR USE   !!
 *                !!   CODES CONCERNING THE CALIBRATION WITHOUT THOROUGH   !!
 *                !!   KNOWLEDGE OF THE CALIBRATION PROCEDURE.             !!
 *                !                                                         ! 
 *                ! M76_STORE_CALI stores all calibration values from       !
 *                !           calibration memory to user EEPROM.            !
 *                !                                                         !
 *                ! M76_DELMAGIC overwrite the magic word in user EEPROM    !
 *                !           with 0xffff. This means that next time        !
 *                !           M76_Init routine runs all calibration values  !
 *                !           in user EEPROM will be set to 0xffff          !
 *                !                                                         !
 *                ! M76_BLK_CALI writes a calibration value for the current !
 *                !           range to calibration memory and writes ADC    !
 *                !           Calibration Registers for the current range.  !
 *                !           Description of data block:                    !
 *                !            data[0]  kind of value                       !
 *                !               0 = zero-scale AC/DC voltage/current      !
 *                !               1 = full-scale AC/DC voltage/current      !
 *                !               2 = Ux zero-scale resistance              !
 *                !               3 = Im full-scale resistance              !
 *                !               4 = Im zero-scale resistance              !
 *                !               5 = Ux full-scale resistance              !
 *                !            data[1]  calibration value                   !
 *                !                                                         !
 *                !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!        
 *
 *                M76_RANGE selects the measuring range
 *                for defines of M76_RANGE_xxx see m76_drv.h and hardware manual
 *
 *                M76_SETTLE defines the time the driver waits after range/
 *                ADC channel was changed.
 *---------------------------------------------------------------------------
 *  Input......:  llHdl             low-level handle
 *                code              status code
 *                ch                current channel
 *                value32_or_64     data or
 *                                  pointer to block data structure (M_SG_BLOCK)  (*)
 *                (*) = for block status codes
 *  Output.....:  return     success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 M76_SetStat(
    LL_HANDLE *llHdl,
    int32  code,
    int32  ch,
    INT32_OR_64 value32_or_64
)
{
    int32       value = (int32)value32_or_64;
    INT32_OR_64 valueP = value32_or_64;
    int32 error = ERR_SUCCESS;

    DBGWRT_1((DBH, "LL - M76_SetStat: ch=%d code=0x%04x value=0x%x\n",
              ch,code,value));

    switch(code) {

        /*--------------------------+
        |  debug level              |
        +--------------------------*/
        case M_LL_DEBUG_LEVEL:
            llHdl->dbgLevel = value;
            break;
        /*--------------------------+
        |  enable interrupts        |
        +--------------------------*/
        case M_MK_IRQ_ENABLE:
            if (value)  {
                llHdl->irqEnable = TRUE;
            }
            else  {
                llHdl->irqEnable = FALSE;
            }
            break;
        /*--------------------------+
        |  set irq counter          |
        +--------------------------*/
        case M_LL_IRQ_COUNT:
            llHdl->irqCount = value;
            break;
        /*--------------------------+
        |  channel direction        |
        +--------------------------*/
        case M_LL_CH_DIR:
            if (value != M_CH_IN)
                error = ERR_LL_ILL_DIR;
            break;
        /*--------------------------+
        |  range                    |
        +--------------------------*/
        case M76_RANGE:
            switch(value) {
            case M76_RANGE_DC_V0:
                llHdl->conMode = DC_V0;
                llHdl->comChan = COM_DC;
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_1;
                llHdl->filPolarity = FHI_POLAR_BI;
                break;

            case M76_RANGE_DC_V1:
                llHdl->conMode = DC_V1;
                llHdl->comChan = COM_DC;
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_1;
                llHdl->filPolarity = FHI_POLAR_BI;
                break;

            case M76_RANGE_DC_V2:
                llHdl->conMode = DC_V2;
                llHdl->comChan = COM_DC;
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_1;
                llHdl->filPolarity = FHI_POLAR_BI;
                break;

            case M76_RANGE_DC_V3:
                llHdl->conMode = DC_V3;
                llHdl->comChan = COM_DC;
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_1;
                llHdl->filPolarity = FHI_POLAR_BI;
                break;

            case M76_RANGE_DC_V4:
                llHdl->conMode = DC_V4;
                llHdl->comChan = COM_DC;
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_1;
                llHdl->filPolarity = FHI_POLAR_BI;
                break;

            case M76_RANGE_AC_V0:
                llHdl->conMode = AC_V0;
                llHdl->comChan = COM_AC;
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_1;
                llHdl->filPolarity = FHI_POLAR_UNI;
                break;

            case M76_RANGE_AC_V1:
                llHdl->conMode = AC_V1;
                llHdl->comChan = COM_AC;
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_1;
                llHdl->filPolarity = FHI_POLAR_UNI;
                break;

            case M76_RANGE_AC_V2:
                llHdl->conMode = AC_V2;
                llHdl->comChan = COM_AC;
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_1;
                llHdl->filPolarity = FHI_POLAR_UNI;
                break;

            case M76_RANGE_AC_V3:
                llHdl->conMode = AC_V3;
                llHdl->comChan = COM_AC;
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_1;
                llHdl->filPolarity = FHI_POLAR_UNI;
                break;

            case M76_RANGE_DC_A0:
                llHdl->conMode = DC_A0;
                llHdl->comChan = COM_DC;
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_1;
                llHdl->filPolarity = FHI_POLAR_BI;
                break;

            case M76_RANGE_DC_A1:
                llHdl->conMode = DC_A1;
                llHdl->comChan = COM_DC;
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_1;
                llHdl->filPolarity = FHI_POLAR_BI;
                break;

            case M76_RANGE_DC_A2:
                llHdl->conMode = DC_A2;
                llHdl->comChan = COM_DC;
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_1;
                llHdl->filPolarity = FHI_POLAR_BI;
                break;

            case M76_RANGE_DC_A3:
                llHdl->conMode = DC_A3;
                llHdl->comChan = COM_DC;
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_1;
                llHdl->filPolarity = FHI_POLAR_BI;
                break;

            case M76_RANGE_AC_A0:
                llHdl->conMode = AC_A0;
                llHdl->comChan = COM_AC;
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_1;
                llHdl->filPolarity = FHI_POLAR_UNI;
                break;

            case M76_RANGE_AC_A1:
                llHdl->conMode = AC_A1;
                llHdl->comChan = COM_AC;
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_1;
                llHdl->filPolarity = FHI_POLAR_UNI;
                break;

            case M76_RANGE_AC_A2:
                llHdl->conMode = AC_A2;
                llHdl->comChan = COM_AC;
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_1;
                llHdl->filPolarity = FHI_POLAR_UNI;
                break;

            case M76_RANGE_R2_0:
                llHdl->conMode = R2_1;  
                llHdl->comChan = COM_R_U;  
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_4; 
                llHdl->filPolarity = FHI_POLAR_UNI;
                break;

            case M76_RANGE_R2_1:
                llHdl->conMode = R2_1;
                llHdl->comChan = COM_R_U;  
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_1;
                llHdl->filPolarity = FHI_POLAR_UNI;
                break;

            case M76_RANGE_R2_2:
                llHdl->conMode = R2_2;
                llHdl->comChan = COM_R_U;  
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_1;
                llHdl->filPolarity = FHI_POLAR_UNI;
                break;

            case M76_RANGE_R2_3:
                llHdl->conMode = R2_3;
                llHdl->comChan = COM_R_U;  
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_1;
                llHdl->filPolarity = FHI_POLAR_UNI;
                break;

            case M76_RANGE_R2_4:
                llHdl->conMode = R2_4;
                llHdl->comChan = COM_R_U;  
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_1;
                llHdl->filPolarity = FHI_POLAR_UNI;
                break;

            case M76_RANGE_R4_0:
                llHdl->conMode = R4_1;   
                llHdl->comChan = COM_R_U;
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_4; 
                llHdl->filPolarity = FHI_POLAR_UNI;
                break;

            case M76_RANGE_R4_1:
                llHdl->conMode = R4_1;
                llHdl->comChan = COM_R_U;  
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_1;
                llHdl->filPolarity = FHI_POLAR_UNI;
                break;

            case M76_RANGE_R4_2:
                llHdl->conMode = R4_2;
                llHdl->comChan = COM_R_U;  
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_1;
                llHdl->filPolarity = FHI_POLAR_UNI;
                break;

            case M76_RANGE_R4_3:
                llHdl->conMode = R4_3;
                llHdl->comChan = COM_R_U;  
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_1;
                llHdl->filPolarity = FHI_POLAR_UNI;
                break;

            case M76_RANGE_R4_4:
                llHdl->conMode = R4_4;
                llHdl->comChan = COM_R_U;  
                llHdl->modMode = MOD_NORMAL;
                llHdl->modGain = MOD_GAIN_1;
                llHdl->filPolarity = FHI_POLAR_UNI;
                break;

            default:
                error = ERR_LL_ILL_PARAM;
                break;
            }
            if (!error)  {
                llHdl->range = value;
                WriteConfigReg(llHdl);
                WriteFilterReg(llHdl);      
                WriteModeReg(llHdl);
                
                WriteCaliRegUpd(llHdl);/* write new cali val to ADC,*/
                                       /*  update cali info */
                OSS_Delay(llHdl->osHdl, llHdl->settleTime);
            }
            break;
        /*-----------------------------------------+
        |   permit measurment with wrong checksum  |
        +-----------------------------------------*/
        case M76_PERMIT:
            if (value)  {
                llHdl->permitMeas = TRUE;
            }
            else  {
                llHdl->permitMeas = FALSE;
            }
            break;
        /*--------------------------+
        |   settling time           |
        +--------------------------*/
        case M76_SETTLE:
            if (value < 0)  {
                error = ERR_LL_ILL_PARAM;
            }
            else  {
                llHdl->settleTime = value;
            }
            break;
        /*--------------------------+
        |  filter frequency         |
        +--------------------------*/
        case M76_FILTER:
            if ((value < 20) || (value > 1920))  {
                error = ERR_LL_ILL_PARAM;
            }
            else  {
                llHdl->filFilter = (u_int16)(value & 0xffff);
                WriteFilterReg(llHdl);
                OSS_Delay(llHdl->osHdl, llHdl->settleTime);
            }
            break;
        /*------------------------------+
        |   write to calibration memory |
        +------------------------------*/
        case M76_BLK_CALI:              
            {
                M_SG_BLOCK *blk = (M_SG_BLOCK*)valueP;
                M76_CALI_VAL *data;

                if (blk->size < sizeof(M76_CALI_VAL))
                    return(ERR_LL_USERBUF);

                data = (M76_CALI_VAL*)blk->data;
                /* write value to memory */
                error = WriteCaliVal(llHdl, data->mode, data->value);
                
                if (!error)  { 
                    WriteCaliRegUpd(llHdl);/* write new cali val to ADC,*/
                                           /*  update cali info */
                }
            }
            break;
        /*--------------------------+
        |   save cali values        |
        +--------------------------*/
        case M76_STORE_CALI:
            if (value != UEE_MAGIC)  {
                error = ERR_LL_ILL_PARAM;
            }
            else  {
                error = WriteCaliProm(llHdl);
                if (error == 0)  {
                    /* valid checksum */
                    llHdl->checkSum = TRUE;
                    llHdl->permitMeas = TRUE;
                }
            }
            break;
        /*--------------------------+
        |   delete uee magic word   |
        +--------------------------*/
        case M76_DELMAGIC:
            if (value != UEE_MAGIC)  {
                error = ERR_LL_ILL_PARAM;
            }
            else  {
                DBGWRT_2((DBH, " delete UEE_MAGIC word\n"));

                MWRITE_D16(llHdl->ma, CTRL_REG, UEPROM_SEL);    /* select user EEPROM */
                error = UeeWrite(llHdl,(u_int8)UEE_MAGIC_ADDRESS, 0xffff);
                MWRITE_D16(llHdl->ma, CTRL_REG, IDPROM_SEL);    /* deselect user EEPROM */
            }
            break;
        /*--------------------------+
        |  (unknown)                |
        +--------------------------*/
        default:
            error = ERR_LL_UNK_CODE;
    }

    return(error);
}

/****************************** M76_GetStat **********************************
 *
 *  Description:  Get the driver status
 *
 *                The following status codes are supported:
 *
 *                Code                 Description                 Values
 *                -------------------  --------------------------  ----------
 *                M_LL_DEBUG_LEVEL     driver debug level          see dbg.h
 *                M_LL_CH_NUMBER       number of channels          1
 *                M_LL_CH_DIR          direction of curr. chan.    M_CH_IN
 *                M_LL_CH_LEN          length of curr. ch. [bits]  32
 *                M_LL_CH_TYP          description of curr. chan.  M_CH_ANALOG
 *                M_LL_IRQ_COUNT       interrupt counter           0..max
 *                M_LL_ID_CHECK        EEPROM is checked           0..1
 *                M_LL_ID_SIZE         EEPROM size [bytes]         128
 *                M_LL_BLK_ID_DATA     EEPROM raw data             -
 *                M_MK_BLK_REV_ID      ident function table ptr    -
 *
 *                M76_RANGE            selected measurement range  M76_RANGE_xxx
 *                M76_CHECKSUM         checksum of calib. values   0..1
 *                M76_SETTLE           settling time               0..max ms
 *                M76_PERMIT           measurement allowed with    0..1
 *                                     wrong checksum
 *                M76_CINFO            calibration info of current 0..1
 *                                     range
 *                M76_CALI          *) calibrates current range    see below
 *
 *                !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
 *                !*) Note:  MEN carries out an initial calibration. The    !
 *                !          values of this procedure are stored in the     !
 *                !          user EEPROM at delivery of the M76 M-Module.   !
 *                !                                                         !  
 *                !!   DO NOT ALTER THE STORED CALIBRATION VALUES OR USE   !!
 *                !!   CODES CONCERNING THE CALIBRATION WITHOUT THOROUGH   !!
 *                !!   KNOWLEDGE OF THE CALIBRATION PROCEDURE.             !!
 *                !                                                         ! 
 *                ! M76_CALI  performs a calibration for the selected range ! 
 *                !           calling value:                                !
 *                !            0 = zero-scale AC/DC voltage/current         !
 *                !            1 = full-scale AC/DC voltage/current         !
 *                !            2 = Ux zero-scale resistance                 !
 *                !            3 = Im full-scale resistance                 !
 *                !            4 = Im zero-scale resistance                 !
 *                !            5 = Ux full-scale resistance                 !
 *                !           modified value:                               !
 *                !            detected calibration value                   !
 *                !                                                         !
 *                !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!        
 *
 *                M76_CHECKSUM compares calibration values with a checksum
 *                    At driver start the calibration values are read from
 *                    the user EEPROM to memory. These values are XORed
 *                    and compared with a checksum also stored in the user
 *                    EEPROM.
 *                     1 .. comparison ok
 *                     0 .. comparison error
 *
 *                M76_CINFO if a word in user EEPROM is 0xffff for the 
 *                     selected range, M76_CINFO returns 0, else 1
 *---------------------------------------------------------------------------
 *  Input......:  llHdl             low-level handle
 *                code              status code
 *                ch                current channel
 *                value32_or_64P    pointer to block data structure (M_SG_BLOCK)  (*) 
 *                                  (*) = for block status codes
 *  Output.....:  value32_or_64P    data pointer or
 *                                  pointer to block data structure (M_SG_BLOCK)  (*) 
 *                return     success (0) or error code
 *                (*) = for block status codes
 *  Globals....:  ---
 ****************************************************************************/
static int32 M76_GetStat(
    LL_HANDLE *llHdl,
    int32  code,
    int32  ch,
    INT32_OR_64  *value32_or_64P
)
{
    int32       *valueP     = (int32*)value32_or_64P; /* pointer to 32 bit value */
    INT32_OR_64 *value64P     = value32_or_64P;         /* stores 32/64bit pointer */
	M_SG_BLOCK  *blk        = (M_SG_BLOCK*)value32_or_64P;

    int32 error = ERR_SUCCESS;   

    DBGWRT_1((DBH, "LL - M76_GetStat: ch=%d code=0x%04x\n",
              ch,code));

    switch(code)
    {
        /*--------------------------+
        |  debug level              |
        +--------------------------*/
        case M_LL_DEBUG_LEVEL:
            *valueP = llHdl->dbgLevel;
            break;
        /*--------------------------+
        |  number of channels       |
        +--------------------------*/
        case M_LL_CH_NUMBER:
            *valueP = CH_NUMBER;
            break;
        /*--------------------------+
        |  channel direction        |
        +--------------------------*/
        case M_LL_CH_DIR:
            *valueP = M_CH_IN;
            break;
        /*--------------------------+
        |  channel length [bits]    |
        +--------------------------*/
        case M_LL_CH_LEN:
            *valueP = 32;
            break;
        /*--------------------------+
        |  channel type info        |
        +--------------------------*/
        case M_LL_CH_TYP:
            *valueP = M_CH_ANALOG;
            break;
        /*--------------------------+
        |  irq counter              |
        +--------------------------*/
        case M_LL_IRQ_COUNT:
            *valueP = llHdl->irqCount;
            break;
        /*--------------------------+
        |  ID PROM check enabled    |
        +--------------------------*/
        case M_LL_ID_CHECK:
            *valueP = llHdl->idCheck;
            break;
        /*--------------------------+
        |   ID PROM size            |
        +--------------------------*/
        case M_LL_ID_SIZE:
            *valueP = MOD_ID_SIZE;
            break;
        /*--------------------------+
        |   ID PROM data            |
        +--------------------------*/
        case M_LL_BLK_ID_DATA:
        {
            error = llHdl->mcrwHdl->ReadEeprom( (void *)llHdl->mcrwHdl,
                                                0, (u_int16*)blk->data,
                                                (u_int16)blk->size );
            break;
        }
        /*--------------------------+
        |   ident table pointer     |
        |   (treat as non-block!)   |
        +--------------------------*/
        case M_MK_BLK_REV_ID:
           *value64P = (INT32_OR_64)&llHdl->idFuncTbl;
           break;
        /*--------------------------+
        |  range                    |
        +--------------------------*/
        case M76_RANGE:
            *valueP = llHdl->range;
            break;
        /*--------------------------+
        |  checksum                 |
        +--------------------------*/
        case M76_CHECKSUM:
            *valueP = llHdl->checkSum;
            break;
        /*--------------------------+
        |  settling time            |
        +--------------------------*/
        case M76_SETTLE:
            *valueP = llHdl->settleTime;
            break;
       /*--------------------------+
        |  filter frequency         |
        +--------------------------*/
        case M76_FILTER:
            *valueP = llHdl->filFilter;
            break;
        /*--------------------------+
        |  permit                   |
        +--------------------------*/
        case M76_PERMIT:
            *valueP = llHdl->permitMeas;
            break;
        /*--------------------------+
        |  cali info                |
        +--------------------------*/
        case M76_CINFO:
            *valueP = llHdl->calibOk;
            break;
        /*--------------------------+
        |   calibration             |
        +--------------------------*/
        case M76_CALI:
            error = CalibAdc(llHdl, valueP);
            break;
        /*--------------------------+
        |  (unknown)                |
        +--------------------------*/
        default:
            error = ERR_LL_UNK_CODE;
    }

    return(error);
}

/******************************* M76_BlockRead *******************************
 *
 *  Description:  Read a data block from the device, to be used for resistance
 *                measurements.
 *                First value is Ux, second value is Im.
 *                For measuring principle see hardware manual.
 *                Valid data is a 24-bit value. The bits are right-aligned in
 *                a 32-bit long word. The upper 8 bits are always zero.
 *                 
 *                  +------+
 *                  |  Ux  |    1st value
 *                  +------+
 *                  |  Im  |    2nd value
 *                  +------+
 *
 *                If the checksum test result is FALSE, an error is returned 
 *                as long as read is not explicitly allowed.
 *                If there are no valid calibration values for the selected 
 *                range an error is returned.
 *---------------------------------------------------------------------------
 *  Input......:  llHdl        low-level handle
 *                ch           current channel
 *                buf          data buffer
 *                size         data buffer size
 *  Output.....:  nbrRdBytesP  number of read bytes
 *                return       success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 M76_BlockRead(
     LL_HANDLE *llHdl,
     int32     ch,
     void      *buf,
     int32     size,
     int32     *nbrRdBytesP
)
{
    int32 error = ERR_SUCCESS;
    int32 *bufP = (int32*)buf;
    u_int16 gain;
 
    DBGWRT_1((DBH, "LL - M76_BlockRead: ch=%d, size=%d\n",ch,size));
    
    if (llHdl->range < M76_RANGE_R2_0)
        return(ERR_LL_ILL_PARAM);
    
    if (size < 8)
        return(ERR_LL_USERBUF);

    if (llHdl->permitMeas == FALSE)     /* wrong checksum */
        return(ERR_LL_DEV_NOTRDY);
    if (llHdl->calibOk == FALSE)        /* not calibrated (Ux) */
        return(ERR_LL_DEV_NOTRDY);
    
    /* get Ux */
    error = ReadDataReg(llHdl, bufP, COM_DATA);
    if (error)
        return(error);
    *bufP >>= 8;
    *bufP &= 0x00ffffff;
    
    /* get Im */
    gain = llHdl->modGain;          /* save Ux gain */
    llHdl->modGain = MOD_GAIN_1;    /* set gain Im */
    llHdl->comChan = COM_R_I;       /* set channel Im */
    WriteFilterReg(llHdl);
    WriteModeReg(llHdl);
    WriteCaliReg(llHdl);

    /* perform a wait ( settling time) */
    OSS_Delay(llHdl->osHdl, llHdl->settleTime);

    bufP++;
    error = ReadDataReg(llHdl, bufP, COM_DATA);
    if (error)
        return(error);
    *bufP >>= 8;
    *bufP &= 0x00ffffff;

    /* default R parameters (Ux) */
    llHdl->modGain = gain;          /* restore Ux gain */
    llHdl->comChan = COM_R_U;       /* set channel Ux */
    WriteFilterReg(llHdl);
    WriteModeReg(llHdl);
    WriteCaliReg(llHdl);
    /* perform a wait ( settling time) */
    OSS_Delay(llHdl->osHdl, llHdl->settleTime);

    /* return number of read bytes */
    *nbrRdBytesP = 8;

    return(ERR_SUCCESS);
}

/****************************** M76_BlockWrite *******************************
 *
 *  Description:  Function not supported on M76.
 *---------------------------------------------------------------------------
 *  Input......:  llHdl        low-level handle
 *                ch           current channel
 *                buf          data buffer
 *                size         data buffer size
 *  Output.....:  nbrWrBytesP  number of written bytes
 *                return       success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 M76_BlockWrite(
     LL_HANDLE *llHdl,
     int32     ch,
     void      *buf,
     int32     size,
     int32     *nbrWrBytesP
)
{
    DBGWRT_1((DBH, "LL - M76_BlockWrite: ch=%d, size=%d\n",ch,size));

    /* return number of written bytes */
    *nbrWrBytesP = 0;

    return(ERR_LL_ILL_FUNC);
}


/****************************** M76_Irq *************************************
 *
 *  Description:  Interrupt service routine.
 *
 *                The interrupt is triggered after read transfer was completed.
 *
 *                If the driver can detect the interrupt's cause it returns
 *                LL_IRQ_DEVICE or LL_IRQ_DEV_NOT, otherwise LL_IRQ_UNKNOWN.
 *---------------------------------------------------------------------------
 *  Input......:  llHdl    low-level handle
 *  Output.....:  return   LL_IRQ_DEVICE    irq caused by device
 *                         LL_IRQ_DEV_NOT   irq not caused by device
 *  Globals....:  ---
 ****************************************************************************/
static int32 M76_Irq(
   LL_HANDLE *llHdl
)
{
    IDBGWRT_1((DBH, ">>> M76_Irq:\n"));

    if ( (MREAD_D16(llHdl->ma, STAT_REG) & IRQ_PEND) == 0 )  
        return(LL_IRQ_DEV_NOT);         /* not my interrupt */

    MWRITE_D16(llHdl->ma, ACCESS_REG, 0);   
                                            
    OSS_SemSignal(llHdl->osHdl, llHdl->readSem);
            
    llHdl->irqCount++;

    return(LL_IRQ_DEVICE);      
}

/****************************** M76_Info ************************************
 *
 *  Description:  Get information about hardware and driver requirements.
 *
 *                The following info codes are supported:
 *
 *                Code                      Description
 *                ------------------------  -----------------------------
 *                LL_INFO_HW_CHARACTER      hardware characteristics
 *                LL_INFO_ADDRSPACE_COUNT   nr of required address spaces
 *                LL_INFO_ADDRSPACE         address space information
 *                LL_INFO_IRQ               interrupt required
 *                LL_INFO_LOCKMODE          process lock mode required
 *
 *                The LL_INFO_HW_CHARACTER code returns all address and 
 *                data modes (ORed) which are supported by the hardware
 *                (MDIS_MAxx, MDIS_MDxx).
 *
 *                The LL_INFO_ADDRSPACE_COUNT code returns the number
 *                of address spaces used by the driver.
 *
 *                The LL_INFO_ADDRSPACE code returns information about one
 *                specific address space (MDIS_MAxx, MDIS_MDxx). The returned 
 *                data mode represents the widest hardware access used by 
 *                the driver.
 *
 *                The LL_INFO_IRQ code returns whether the driver supports an
 *                interrupt routine (TRUE or FALSE).
 *
 *                The LL_INFO_LOCKMODE code returns which process locking
 *                mode the driver needs (LL_LOCK_xxx).
 *---------------------------------------------------------------------------
 *  Input......:  infoType     info code
 *                ...          argument(s)
 *  Output.....:  return       success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 M76_Info(
   int32  infoType,
   ...
)
{
    int32   error = ERR_SUCCESS;
    va_list argptr;

    va_start(argptr, infoType );

    switch(infoType) {
        /*-------------------------------+
        |  hardware characteristics      |
        |  (all addr/data modes ORed)    |
        +-------------------------------*/
        case LL_INFO_HW_CHARACTER:
        {
            u_int32 *addrModeP = va_arg(argptr, u_int32*);
            u_int32 *dataModeP = va_arg(argptr, u_int32*);

            *addrModeP = MDIS_MA08;
            *dataModeP = MDIS_MD08 | MDIS_MD16;
            break;
        }
        /*-------------------------------+
        |  nr of required address spaces |
        |  (total spaces used)           |
        +-------------------------------*/
        case LL_INFO_ADDRSPACE_COUNT:
        {
            u_int32 *nbrOfAddrSpaceP = va_arg(argptr, u_int32*);

            *nbrOfAddrSpaceP = ADDRSPACE_COUNT;
            break;
        }
        /*-------------------------------+
        |  address space type            |
        |  (widest used data mode)       |
        +-------------------------------*/
        case LL_INFO_ADDRSPACE:
        {
            u_int32 addrSpaceIndex = va_arg(argptr, u_int32);
            u_int32 *addrModeP = va_arg(argptr, u_int32*);
            u_int32 *dataModeP = va_arg(argptr, u_int32*);
            u_int32 *addrSizeP = va_arg(argptr, u_int32*);

            if (addrSpaceIndex >= ADDRSPACE_COUNT)
                error = ERR_LL_ILL_PARAM;
            else {
                *addrModeP = MDIS_MA08;
                *dataModeP = MDIS_MD16;
                *addrSizeP = ADDRSPACE_SIZE;
            }

            break;
        }
        /*-------------------------------+
        |   interrupt required           |
        +-------------------------------*/
        case LL_INFO_IRQ:
        {
            u_int32 *useIrqP = va_arg(argptr, u_int32*);

            *useIrqP = USE_IRQ;
            break;
        }
        /*-------------------------------+
        |   process lock mode            |
        +-------------------------------*/
        case LL_INFO_LOCKMODE:
        {
            u_int32 *lockModeP = va_arg(argptr, u_int32*);

            *lockModeP = LL_LOCK_CALL;
            break;
        }
        /*-------------------------------+
        |   (unknown)                    |
        +-------------------------------*/
        default:
          error = ERR_LL_ILL_PARAM;
    }

    va_end(argptr);
    return(error);
}

/*******************************  Ident  ************************************
 *
 *  Description:  Return ident string.
 *---------------------------------------------------------------------------
 *  Input......:  -
 *  Output.....:  return  pointer to ident string
 *  Globals....:  -
 ****************************************************************************/
static char* Ident( void )  /* nodoc */
{
    return( "M76 - M76 low level driver: $Id: m76_drv.c,v 1.8 2010/03/11 17:04:34 amorbach Exp $" );
}

/********************************* Cleanup **********************************
 *
 *  Description: Close all handles, free memory and return error code.
 *               Note: The low-level handle is invalid after this function is
 *                     called.
 *---------------------------------------------------------------------------
 *  Input......: llHdl      low-level handle
 *               retCode    return value
 *  Output.....: return     retCode
 *  Globals....: -
 ****************************************************************************/
static int32 Cleanup(
   LL_HANDLE    *llHdl,
   int32        retCode     /* nodoc */
)
{
    /*------------------------------+
    |  close handles                |
    +------------------------------*/
    /* clean up desc */
    if (llHdl->descHdl)
        DESC_Exit(&llHdl->descHdl);

    if (llHdl->readSem)
        OSS_SemRemove( llHdl->osHdl, &llHdl->readSem );

    if( llHdl->mcrwHdl )
        llHdl->mcrwHdl->Exit( (void **)&llHdl->mcrwHdl );
    
    /* clean up debug */
    DBGEXIT((&DBH));

    /*------------------------------+
    |  free memory                  |
    +------------------------------*/
    /* free my handle */
    OSS_MemFree(llHdl->osHdl, (int8*)llHdl, llHdl->memAlloc);

    /*------------------------------+
    |  return error code            |
    +------------------------------*/
    return(retCode);
}

/********************************* MakeMwHandle ****************************
 *
 *  Description: Make the microwire handle to read IDPROM.
 *---------------------------------------------------------------------------
 *  Input......: llHdl      low-level handle
 *  Output.....: 
 *  Globals....: -
 ****************************************************************************/
static int32 MakeMwHandle( LL_HANDLE *llHdl )/* nodoc */
{
    MCRW_DESC_PORT descMcrw, *desc = &descMcrw;
    int32 error;
    MACCESS ma2;

    /* bus speed */
    desc->busClock   = 10; /* OSS_MikroDelay */

    /* address length */
    desc->addrLength   = 6; /* for 93C46 */

    /* set FLAGS */
    desc->flagsDataIn   = 
        ( MCRW_DESC_PORT_FLAG_SIZE_16 | MCRW_DESC_PORT_FLAG_READABLE_REG | 
          MCRW_DESC_PORT_FLAG_POLARITY_HIGH );

    desc->flagsDataOut  =
        ( MCRW_DESC_PORT_FLAG_SIZE_16 | MCRW_DESC_PORT_FLAG_POLARITY_HIGH );

    desc->flagsClockOut =
        ( MCRW_DESC_PORT_FLAG_SIZE_16 | MCRW_DESC_PORT_FLAG_POLARITY_HIGH );

    desc->flagsCsOut =
        ( MCRW_DESC_PORT_FLAG_SIZE_16 | MCRW_DESC_PORT_FLAG_POLARITY_HIGH );

    desc->flagsOut   = MCRW_DESC_PORT_FLAG_OUT_IN_ONE_REG;

    /* set addr and mask */
    MACCESS_CLONE( llHdl->ma, ma2, PLD_IF_REG );
    desc->addrDataIn = (void *)ma2;
    desc->maskDataIn   = 0x0001; /*EEDAT*/

    desc->addrDataOut = (void *)ma2;
    desc->maskDataOut  = 0x0001; /*EEDAT*/
    desc->notReadBackDefaultsDataOut = 0xFFFE;
    desc->notReadBackMaskDataOut     = 0xFFFE;

    desc->addrClockOut = (void *)ma2;
    desc->maskClockOut = 0x0002; /*EECLK*/
    desc->notReadBackDefaultsClockOut = 0xFFFD;
    desc->notReadBackMaskClockOut     = 0xFFFD;
    
    desc->addrCsOut = (void *)ma2;
    desc->maskCsOut  = 0x0004; /*EECS*/
    desc->notReadBackDefaultsCsOut = 0xFFFB;
    desc->notReadBackMaskCsOut     = 0xFFFB;

    error = MCRW_PORT_Init( &descMcrw,
                            llHdl->osHdl,
                            (void **)&llHdl->mcrwHdl );
    if( error || llHdl->mcrwHdl == NULL ){
        DBGWRT_ERR( ( DBH, " *** M76_Init: MCRW_PORT_Init() error=%d\n",
                      error ));
        error = ERR_ID;
    }
    return error;
}


/********************************* WriteConfigReg *****************************
 *
 *  Description: Write to Config Register.
 *---------------------------------------------------------------------------
 *  Input......: llHdl      low-level handle
 *  Output.....: return     0
 *  Globals....: -
 ****************************************************************************/
static int32 WriteConfigReg(LL_HANDLE *llHdl)   /* nodoc */
{
    u_int16 conH,conL;

    conH = (u_int16)(llHdl->conMode >> 16);
    MWRITE_D16(llHdl->ma, CONFIG_REG, conH);    /* high word */

    conL = (u_int16)(llHdl->conMode & 0xffff);
    MWRITE_D16(llHdl->ma, CONFIG_REG+2, conL);  /* low word */

    DBGWRT_2((DBH, "LL - WriteConfigReg: %x, %x\n",conH,conL));

    return (0);
}


/********************************* WriteModeReg *****************************
 *
 *  Description: Write to Mode Register.
 *---------------------------------------------------------------------------
 *  Input......: llHdl      low-level handle
 *  Output.....: return     0
 *  Globals....: -
 ****************************************************************************/
static int32 WriteModeReg(LL_HANDLE *llHdl) /* nodoc */
{
    u_int16 com, mod;

    com = (COM_MODE | llHdl->comChan);
    MWRITE_D16(llHdl->ma, COM_REG, com);
    
    mod = (llHdl->modMode | llHdl->modGain);
    MWRITE_D16(llHdl->ma, COM_REG, mod);

    DBGWRT_2((DBH, "LL - WriteModeReg: %x, %x\n",com,mod));

    return(0);
}


/********************************* WriteFilterReg ***************************
 *
 *  Description: Write to Filter Registers.
 *---------------------------------------------------------------------------
 *  Input......: llHdl      low-level handle
 *  Output.....: return     0
 *  Globals....: -
 ****************************************************************************/
static int32 WriteFilterReg(LL_HANDLE *llHdl)   /* nodoc */
{
    u_int16 com,fil;
    
    /* filter high */
    com = (COM_FILTER_HIGH | llHdl->comChan);
    MWRITE_D16(llHdl->ma, COM_REG, com);
    
    fil = ( llHdl->filPolarity | FHI_WL | ((llHdl->filFilter>>8) & 0xf) );
    MWRITE_D16(llHdl->ma, COM_REG, fil);

    DBGWRT_2((DBH, "LL - WriteFilterReg high: %x, %x\n",com,fil));

    /* filter low */
    com = (COM_FILTER_LOW | llHdl->comChan);
    MWRITE_D16(llHdl->ma, COM_REG, com);
    
    fil = (llHdl->filFilter & 0xff);
    MWRITE_D16(llHdl->ma, COM_REG, fil);

    DBGWRT_2((DBH, "LL - WriteFilterReg low: %x, %x\n",com,fil));

    return(0);
}


/********************************* CalibAdc *********************************
 *
 *  Description: Performs calibration for current range. 
 *---------------------------------------------------------------------------
 *  Input......: llHdl  low-level handle
 *               val    0 = zero-scale AC/DC voltage/current
 *                      1 = full-scale AC/DC voltage/current
 *                      2 = Ux zero-scale resistance
 *                      3 = Im full-scale resistance
 *                      4 = Im zero-scale resistance
 *                      5 = Ux full-scale resistance
 *  Output.....: return     success (0) or error code
 *               llHdl      calib value in llHdl for range and val
 *               val        calib value vor range and val
 *  Globals....: -
 ****************************************************************************/
static int32 CalibAdc(LL_HANDLE *llHdl, int32 *val) /* nodoc */
{
    int32 error=0;

    DBGWRT_2((DBH, "LL - CalibAdc val: %x\n",*val));
    
    if (*val < 0)
        return(ERR_LL_ILL_PARAM);
    if (*val > 5)
        return(ERR_LL_ILL_PARAM);
    if ( (*val < 2) && (llHdl->range > M76_RANGE_AC_A2) )
        return(ERR_LL_ILL_PARAM);
    if ( (*val > 1) && (llHdl->range < M76_RANGE_R2_0) )
        return(ERR_LL_ILL_PARAM);

    /* detect cali values for range */
    if (llHdl->range < M76_RANGE_R2_0)  {  /* AC + DC calibration */
        switch (*val)  {
        case 0: /* cali zero-scale */
            /* initiate calibration */
            llHdl->modMode = MOD_ZERO;
            WriteModeReg(llHdl);
            llHdl->modMode = MOD_NORMAL;

            OSS_Delay(llHdl->osHdl, 1);     /* wait at least one modulator cycle */ 
            
            error = ReadDataReg(llHdl, val, COM_DATA);
            if (error == 0)
                error = ReadDataReg(llHdl, val, COM_CALI_ZERO);
            break;
        
        case 1: /* cali full-scale */
            /* initiate calibration */
            llHdl->modMode = MOD_FULL;
            WriteModeReg(llHdl);
            llHdl->modMode = MOD_NORMAL;

            OSS_Delay(llHdl->osHdl, 1);     /* wait at least one modulator cycle */ 
            
            error = ReadDataReg(llHdl, val, COM_DATA);
            if (error == 0)
                error = ReadDataReg(llHdl, val, COM_CALI_FULL);
            break;
        default:
            error = ERR_LL_ILL_PARAM;
        }
    }
    else  {     /* resistor calibration (range > M76_RANGE_AC_A2) */
        switch(*val)  {
        case 4: /* Im cali zero-scale */
            {
            u_int16 gain;

            /* special Im parameters */
            gain = llHdl->modGain;  /* store gain for Ux */
            llHdl->modGain = MOD_GAIN_1;
            llHdl->comChan = COM_R_I;
            /* perform some normal conversions for a time */
            WriteFilterReg(llHdl);
            WriteModeReg(llHdl);  
            OSS_Delay(llHdl->osHdl, llHdl->settleTime); /* channel changed */

            /* initiate calibration */
            llHdl->modMode = MOD_ZERO;
            WriteModeReg(llHdl);            

            OSS_Delay(llHdl->osHdl, 1);     /* wait at least one modulator cycle */ 
            
            error = ReadDataReg(llHdl, val, COM_DATA);
            if (error == 0)
                error = ReadDataReg(llHdl, val, COM_CALI_ZERO );

            /* restore Ux settings in llHdl */
            llHdl->modMode = MOD_NORMAL;    
            llHdl->comChan = COM_R_U;
            llHdl->modGain = gain;

            /* set ADC to default R parameters (Ux) */
            WriteFilterReg(llHdl);
            WriteModeReg(llHdl);  
            WriteCaliReg(llHdl);
            OSS_Delay(llHdl->osHdl, llHdl->settleTime);
            break;
            }

        case 3: /* Im cali full-scale */
            {
            u_int16 gain;
            /* special Im parameters */
            gain = llHdl->modGain;  /* store gain for Ux */
            llHdl->modGain = MOD_GAIN_1;
            llHdl->comChan = COM_R_I;
            /* perform some normal conversions for a time */
            WriteFilterReg(llHdl);
            WriteModeReg(llHdl);  
            WriteCaliReg(llHdl);    /* write Im zero-scale calibration val */
            OSS_Delay(llHdl->osHdl, llHdl->settleTime); /* channel changed */

            /* initiate calibration */
            llHdl->modMode = MOD_FULL;
            WriteModeReg(llHdl);            

            OSS_Delay(llHdl->osHdl, 1);     /* wait at least one modulator cycle */ 
            
            error = ReadDataReg(llHdl, val, COM_DATA);
            if (error == 0)
                error = ReadDataReg(llHdl, val, COM_CALI_FULL );

            /* restore Ux settings in llHdl */
            llHdl->modMode = MOD_NORMAL;    
            llHdl->comChan = COM_R_U;
            llHdl->modGain = gain;

            /* set ADC to default R parameters (Ux) */
            WriteFilterReg(llHdl);
            WriteModeReg(llHdl);  
            WriteCaliReg(llHdl);
            OSS_Delay(llHdl->osHdl, llHdl->settleTime);
            break;
            }

        case 2: /* Ux cali zero-scale */
            /* initiate calibration */
            llHdl->modMode = MOD_ZERO;
            WriteModeReg(llHdl);
            llHdl->modMode = MOD_NORMAL;

            OSS_Delay(llHdl->osHdl, 1);     /* wait at least one modulator cycle */ 
            
            error = ReadDataReg(llHdl, val, COM_DATA);
            if (error == 0)
                error = ReadDataReg(llHdl, val, COM_CALI_ZERO );
            break;

        case 5: /* Ux cali full-scale */
            /* initiate calibration */
            llHdl->modMode = MOD_FULL;
            WriteModeReg(llHdl);
            llHdl->modMode = MOD_NORMAL;

            OSS_Delay(llHdl->osHdl, 1);     /* wait at least one modulator cycle */ 
            
            error = ReadDataReg(llHdl, val, COM_DATA);
            if (error == 0)
                error = ReadDataReg(llHdl, val, COM_CALI_FULL );
            break;
        default:
            error = ERR_LL_ILL_PARAM;
        }

    }
    return(error);
}


/********************************* WriteCaliVal *****************************
 *
 *  Description: Write a value to calibration memory for current range.
 *---------------------------------------------------------------------------
 *  Input......: llHdl  low-level handle
 *               mode   0 = zero-scale AC/DC voltage/current
 *                      1 = full-scale AC/DC voltage/current
 *                      2 = Ux zero-scale resistance
 *                      3 = Im full-scale resistance
 *                      4 = Im zero-scale resistance
 *                      5 = Ux full-scale resistance
 *               val  value to be written 
 *  Output.....: return     success (0) or error code
 *               llHdl      calib value in llHdl for range and val
 *               val        calib value vor range and val
 *  Globals....: -
 ****************************************************************************/
static int32 WriteCaliVal(LL_HANDLE *llHdl, u_int32 mode, u_int32 val)/* nodoc */
{
    CALI_VA *va;
    CALI_R  *r;
    u_int32 i;
    
    DBGWRT_2((DBH, "LL - WriteCaliVal mode: %x  val: %x\n",mode,val));

    if (mode > 5)
        return(ERR_LL_ILL_PARAM);
    if ( (mode < 2) && (llHdl->range > M76_RANGE_AC_A2) )
        return(ERR_LL_ILL_PARAM);
    if ( (mode > 1) && (llHdl->range < M76_RANGE_R2_0) )
        return(ERR_LL_ILL_PARAM);

    if ( ((val & 0xffff0000) == 0xffff0000)  ||
         ((val & 0x0000ffff) == 0x0000ffff) )
        return(ERR_LL_ILL_PARAM);

    /* detect cali values for range */
    if (llHdl->range < M76_RANGE_R2_0)  {  /* AC + DC calibration */
        va = &llHdl->caliVals.dcV[0];
    
        for(i=M76_RANGE_DC_V0; i<llHdl->range; i++, va++); /* go to start of current */
                                                       /* calib parameters */   
        switch (mode)  {
        case 0: /* cali zero-scale */
            va->zero = val;
            break;
        
        case 1: /* cali full-scale */
            va->full = val;
            break;
        default:
            return(ERR_LL_ILL_PARAM);
        }
    }
    else  {     /* resistor calibration (range > M76_RANGE_AC_A2) */
        r = &llHdl->caliVals.r2[0];
        /* go to start of current calib parameters */
        for( i=M76_RANGE_R2_0; i<llHdl->range; i++, r++); 
                                                                        
        switch(mode)  {
        case 2: /* Ux cali zero-scale */
            r->rShort.zero = val;
            break;

        case 5: /* Ux cali full-scale */
            r->rOpen.full  = val;
            break;

        case 4: /* Im cali zero-scale */
            r->rOpen.zero = val;
            break;

        case 3: /* Im cali full-scale */
            r->rShort.full = val;
            break;
        default:
            return(ERR_LL_ILL_PARAM);
        }

    }
    return(0);
}


/********************************* WriteCaliReg ***************************
 *
 *  Description: Write calibration values for current range from calibration 
 *               memory to Calibration Register of ADC.
 *               Test validity of values.
 *---------------------------------------------------------------------------
 *  Input......: llHdl      low-level handle
 *  Output.....: return     0 or error code if values invalid
 *  Globals....: -
 ****************************************************************************/
static int32 WriteCaliReg(LL_HANDLE *llHdl) /* nodoc */
{
    CALI_VA *va;
    CALI_R  *r;
    u_int32 i;
    int32 error=0;
    u_int16 com, calH, calL;

    /* get cali values for range */
    if (llHdl->range < M76_RANGE_R2_0)  {  /* AC + DC measurement */
        va = &llHdl->caliVals.dcV[0];
    
        for(i=M76_RANGE_DC_V0; i<llHdl->range; i++, va++); /* go to start of current */
                                                       /* calib parameters */   
        /*----------------+
        | cali zero-scale |
        +----------------*/
        com = (COM_CALI_ZERO | llHdl->comChan);
        MWRITE_D16(llHdl->ma, COM_REG, com);
    
        calH = (u_int16)(va->zero >> 16);
        MWRITE_D16(llHdl->ma, DATA_REG, calH);  /* high word */
        calL = (u_int16)(va->zero & 0xffff);
        MWRITE_D16(llHdl->ma, DATA_REG+2, calL);/* low word */
        /* check validity*/
        if ((calH==0xffff) || (calL==0xffff))  
            error = ERR_LL_ILL_PARAM;
        DBGWRT_2((DBH, "LL - WriteCaliReg zero (V/A): %x %x %x\n",com,calH,calL));

        /*----------------+
        | cali full-scale |
        +----------------*/
        com = (COM_CALI_FULL | llHdl->comChan);
        MWRITE_D16(llHdl->ma, COM_REG, com);
    
        calH = (u_int16)(va->full >> 16);
        MWRITE_D16(llHdl->ma, DATA_REG, calH);
        calL = (u_int16)(va->full & 0xffff);
        MWRITE_D16(llHdl->ma, DATA_REG+2, calL);/* low word */
        /* check validity*/
        if ((calH==0xffff) || (calL==0xffff))
            error = ERR_LL_ILL_PARAM;
        DBGWRT_2((DBH, "LL - WriteCaliReg full (V/A): %x %x %x\n",com,calH,calL));

    }
    else  {         /* R measurement */
        r = &llHdl->caliVals.r2[0];
        
        for( i=M76_RANGE_R2_0; i<llHdl->range; i++, r++); /* go to start of current */
                                                      /* calib parameters */                    

        if (llHdl->comChan == COM_R_I)  {
            /*--------------------+ 
            | cali Im, zero-scale |
            +--------------------*/
            com = (COM_CALI_ZERO | COM_R_I);
            MWRITE_D16(llHdl->ma, COM_REG, com);
        
            calH = (u_int16)(r->rOpen.zero >> 16);
            MWRITE_D16(llHdl->ma, DATA_REG, calH);
            calL = (u_int16)(r->rOpen.zero & 0xffff);
            MWRITE_D16(llHdl->ma, DATA_REG+2, calL);/* low word */
            /* check validity*/
            if ((calH==0xffff) || (calL==0xffff))
                error = ERR_LL_ILL_PARAM;

            DBGWRT_2((DBH, "LL - WriteCaliReg zero (R,Im): %x %x %x\n",com,calH,calL));

            /*--------------------+
            | cali Im, full-scale |
            +--------------------*/
            com = (COM_CALI_FULL | COM_R_I);
            MWRITE_D16(llHdl->ma, COM_REG, com);
        
            calH = (u_int16)(r->rShort.full >> 16);
            MWRITE_D16(llHdl->ma, DATA_REG, calH);
            calL = (u_int16)(r->rShort.full & 0xffff);
            MWRITE_D16(llHdl->ma, DATA_REG+2, calL);/* low word */
            /* check validity*/
            if ((calH==0xffff) || (calL==0xffff))
                error = ERR_LL_ILL_PARAM;

            DBGWRT_2((DBH, "LL - WriteCaliReg full (R,Im): %x %x %x\n",com,calH,calL));
        }
        else  {
            /*--------------------+
            | cali Ux, zero-scale |
            +--------------------*/ 
            com = (COM_CALI_ZERO | COM_R_U);
            MWRITE_D16(llHdl->ma, COM_REG, com);
        
            calH = (u_int16)(r->rShort.zero >> 16);
            MWRITE_D16(llHdl->ma, DATA_REG, calH);
            calL = (u_int16)(r->rShort.zero & 0xffff);
            MWRITE_D16(llHdl->ma, DATA_REG+2, calL);/* low word */
            /* check validity*/
            if ((calH==0xffff) || (calL==0xffff))
                error = ERR_LL_ILL_PARAM;

            DBGWRT_2((DBH, "LL - WriteCaliReg zero (R,Ux): %x %x %x\n",com,calH,calL));
            /*--------------------+
            | cali Ux, full-scale |
            +--------------------*/
            com = (COM_CALI_FULL | COM_R_U);
            MWRITE_D16(llHdl->ma, COM_REG, com);
        
            calH = (u_int16)(r->rOpen.full >> 16);
            MWRITE_D16(llHdl->ma, DATA_REG, calH);
            calL = (u_int16)(r->rOpen.full & 0xffff);
            MWRITE_D16(llHdl->ma, DATA_REG+2, calL);/* low word */
            /* check validity*/
            if ((calH==0xffff) || (calL==0xffff))
                error = ERR_LL_ILL_PARAM;

            DBGWRT_2((DBH, "LL - WriteCaliReg full (R,Ux): %x %x %x\n",com,calH,calL));
        }
        
    }
    return(error);
}


/********************************* WriteCaliRegUpd **************************
 *
 *  Description: Write calibration values for current range from calibration 
 *               memory to Calibration Register of ADC (call WriteCaliReg) 
 *               and updates calibration info in llHdl.
 *---------------------------------------------------------------------------
 *  Input......: llHdl      low-level handle
 *  Output.....: return     0
 *               llHdl      updated
 *  Globals....: -
 ****************************************************************************/
static int32 WriteCaliRegUpd(LL_HANDLE *llHdl)  /* nodoc */
{
    /* if range = R check validity of calibration vals for Im and Ux*/
    if (llHdl->range > M76_RANGE_AC_A2)  {
        u_int32 help;
        /* check Im calibration vals */
        llHdl->comChan = COM_R_I;
        if (WriteCaliReg(llHdl))
            help = FALSE;
        else 
            help = TRUE;
        /* check and write Ux calibration vals */
        llHdl->comChan = COM_R_U; /* default */
        if (WriteCaliReg(llHdl) || (help==FALSE))  {
            llHdl->calibOk = FALSE;
            DBGWRT_2((DBH, "     calib vals = FALSE\n"));
        }
        else  {
            llHdl->calibOk = TRUE;
            DBGWRT_2((DBH, "     calib vals = TRUE\n"));
        }
    }
    /* range U, I */
    else  {
        if (WriteCaliReg(llHdl))  {
            llHdl->calibOk = FALSE;
            DBGWRT_2((DBH, "     calib vals = FALSE\n"));
        }
        else  {
            llHdl->calibOk = TRUE;
            DBGWRT_2((DBH, "     calib vals = TRUE\n"));
        }
    }
    return(0);
}

/********************************* ReadCaliProm *****************************
 *
 *  Description: Read calibration values from user EEPROM to calibration 
 *               memory and compare with checksum.
 *---------------------------------------------------------------------------
 *  Input......: llHdl      low-level handle
 *  Output.....: llHdl->caliVals   read calibration words 
 *               return     success (0) or error code if invalid checksum
 *  Globals....: -
 ****************************************************************************/
static int32 ReadCaliProm(LL_HANDLE *llHdl) /* nodoc */
{
    int32 error=0;
    u_int16 idx, checkSum = 0, help;
    u_int16 val16;
    int32 *vals = (int32*)&llHdl->caliVals;

    DBGWRT_2((DBH, "LL - ReadCaliProm\n"));
    
    MWRITE_D16(llHdl->ma, CTRL_REG, UEPROM_SEL);    /* select user EEPROM */
    
    for (idx=0; idx < (sizeof(llHdl->caliVals)/2); idx+=2 )  {
        val16 = (u_int16) __M76_UeeRead(llHdl->osHdl,
                                      llHdl->ma, (u_int8)idx);
        checkSum ^= val16;
        *vals = val16;
        val16 = (u_int16) __M76_UeeRead(llHdl->osHdl, 
                                      llHdl->ma, (u_int8)idx+1);
        checkSum ^= val16;
        *vals = *vals | ((u_int32)val16 << 16 );

        DBGWRT_3((DBH, "  cali val %x:  %08x\n",idx,*vals));
        vals++;
    }
    help = (u_int16) __M76_UeeRead(llHdl->osHdl, 
           llHdl->ma, (u_int8)idx);

    DBGWRT_3((DBH, "  uee Checksum: %x,  calculated checksum: %x\n",help, checkSum));

    /* compare checksum */
    if (checkSum != help)
        error = ERR_LL_READ;    /* invalid checksum */

    MWRITE_D16(llHdl->ma, CTRL_REG, IDPROM_SEL);    /* deselect user EEPROM */

    return (error);
}


/********************************* WriteCaliProm *****************************
 *
 *  Description: Write calibration values to user EEPROM 
 *               and build an XORed checksum. 
 *---------------------------------------------------------------------------
 *  Input......: llHdl      low-level handle
 *  Output.....: return     success (0) or error code
 *  Globals....: -
 ****************************************************************************/
static int32 WriteCaliProm(LL_HANDLE *llHdl)    /* nodoc */
{
    int32 error=0;
    int32 *vals = (int32*)&llHdl->caliVals;
    u_int16 idx, checkSum = 0;
    u_int16 val16;

    DBGWRT_2((DBH, "LL - WriteCaliProm\n"));

    MWRITE_D16(llHdl->ma, CTRL_REG, UEPROM_SEL);    /* select user EEPROM */

    for (idx=0; idx < (sizeof(llHdl->caliVals)/2); idx++ )  {
        
        DBGWRT_3((DBH, "  cali val %x:  %08x\n",idx,*vals));
        
        val16 = (u_int16)(*vals & 0xffff);
        checkSum ^= val16;

        error = UeeWrite(llHdl, (u_int8)idx, val16);
        if( error == 0 ){
            val16 = (u_int16)((*vals >> 16) & 0xffff);
            checkSum ^= val16;
            idx++;
            error = UeeWrite(llHdl, (u_int8)idx, val16);
        }
        if (error)  {
            break;
        }
        vals++;
    }
    if (error == 0)  {

        DBGWRT_3((DBH, "  calculated checksum: %x\n",checkSum));
        
        /* write checksum */
        error = UeeWrite(llHdl, (u_int8)idx, checkSum);
    }
    
    MWRITE_D16(llHdl->ma, CTRL_REG, IDPROM_SEL);    /* deselect user EEPROM */

    return (error);
}


/********************************* UeeWrite ********************************
 *
 *  Description: Write a value to user EEPROM at index
 *---------------------------------------------------------------------------
 *  Input......: llHdl      low-level handle
 *  Output.....: return     success (0) or error code
 *  Globals....: -
 ****************************************************************************/
static int32 UeeWrite(LL_HANDLE *llHdl, u_int8 index, u_int16 value)/* nodoc */
{
    int32 error=0,cycle=10;

    /* 
     * try several times to write, as with high system load you might 
     * not detect final busy signal from eeprom 
     */
    do {    
        error = 0;
        cycle--;
        error = __M76_UeeWrite(llHdl->osHdl, llHdl->ma,
                            index, value);
        if (error == 2)  /* verify error */
            break;
    } while (error && (cycle>0)); 

    DBGWRT_3((DBH, "  UeeWrite: idx=%x, value=%04x, uee err no=%x, cycle=%d\n",
                      index,value,error,cycle));

    if (error)  {
        error = ERR_LL_WRITE;
    }

    return (error);
}

/********************************* ReadDataReg ******************************
 *
 *  Description: Read data Register.
 *               If interrupt is enabled then wait for readSem
 *               else poll TRDYR flag.
 *---------------------------------------------------------------------------
 *  Input......: llHdl      low-level handle 
 *               reg        ADC register to be read via data register
 *  Output.....: value      read value
 *               return     success (0) or error code
 *  Globals....: -
 ****************************************************************************/
static int32 ReadDataReg(LL_HANDLE *llHdl, int32 *value, int32 reg) /* nodoc */
{
    int32 error;
    u_int32 i=0;
    u_int16 com, acc;

    /* next operation is a read from the Data Register */
    com = (u_int16)(reg | COM_READ | llHdl->comChan);
    MWRITE_D16(llHdl->ma, COM_REG, com);

    if (llHdl->irqEnable)  {        /* read using interrupt */
        acc = (TR24R | IRQ);
        MWRITE_D16(llHdl->ma, ACCESS_REG, acc);

        DBGWRT_2((DBH, "LL - ReadDataReg (int): %x %x\n",com,acc));

        error = OSS_SemWait( llHdl->osHdl, llHdl->readSem, 2000);
        if (error)
            return (error);
    
        *value = (MREAD_D16(llHdl->ma, DATA_REG) << 16);
        *value |= MREAD_D16(llHdl->ma, DATA_REG+2);
    }
    else  {                         /* read polling */
        acc = (TR24R);
        MWRITE_D16(llHdl->ma, ACCESS_REG, acc);

        DBGWRT_2((DBH, "LL - ReadDataReg (pol): %x %x\n",com,acc));

        while  ((MREAD_D16(llHdl->ma, STAT_REG) & TRDYR) == 0 )  {
            OSS_Delay(llHdl->osHdl,10);
            i++;
            if (i >= POLL_TOUT)  {
                return(ERR_LL_DEV_NOTRDY);
            }
        }
        *value = (MREAD_D16(llHdl->ma, DATA_REG) << 16);
        *value |= MREAD_D16(llHdl->ma, DATA_REG+2);
    }   
    return(ERR_SUCCESS);
}


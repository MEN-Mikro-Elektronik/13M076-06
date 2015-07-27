/****************************************************************************
 ************                                                    ************
 ************                   M76_SIMP                         ************
 ************                                                    ************
 ****************************************************************************
 *  
 *       Author: ls
 *        $Date: 2010/03/11 17:04:08 $
 *    $Revision: 1.6 $
 *
 *  Description: Simple example program for the M76 driver
 *
 *               Reads a value in DC voltage range, 12.5V
 *                      
 *     Required: libraries: mdis_api
 *     Switches: -
 *
 *-------------------------------[ History ]---------------------------------
 *
 * $Log: m76_simp.c,v $
 * Revision 1.6  2010/03/11 17:04:08  amorbach
 * R: Porting to MDIS5
 * M: changed according to MDIS Porting Guide 0.8
 *
 * Revision 1.5  2002/09/04 14:01:01  LSchoberl
 * cosmetics
 *
 * Revision 1.4  2002/08/27 11:47:35  LSchoberl
 * "usage" changed
 *
 * Revision 1.3  2001/12/18 14:33:12  Schoberl
 * uses defines for range with M76_ prefix
 *
 * Revision 1.2  2001/06/08 10:20:41  kp
 * cosmetics
 *
 * Revision 1.1  2001/06/05 16:24:17  Schoberl
 * Initial Revision
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 2010 by MEN Mikro Elektronik GmbH, Nuremberg, Germany 
 ****************************************************************************/
 
static const char RCSid[]="$Id: m76_simp.c,v 1.6 2010/03/11 17:04:08 amorbach Exp $";

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <MEN/men_typs.h>
#include <MEN/mdis_api.h>
#include <MEN/usr_oss.h>
#include <MEN/m76_drv.h>

/*--------------------------------------+
|   DEFINES                             |
+--------------------------------------*/
#define FS		16777215.0   /* 0x00ffffff */

/*--------------------------------------+
|   TYPDEFS                             |
+--------------------------------------*/
/* none */

/*--------------------------------------+
|   EXTERNALS                           |
+--------------------------------------*/
/* none */

/*--------------------------------------+
|   GLOBALS                             |
+--------------------------------------*/
/* none */

/*--------------------------------------+
|   PROTOTYPES                          |
+--------------------------------------*/
static void PrintError(char *info);


/********************************* main *************************************
 *
 *  Description: Program main function
 *			   
 *---------------------------------------------------------------------------
 *  Input......: argc,argv	argument counter, data ..
 *  Output.....: return	    success (0) or error (1)
 *  Globals....: -
 ****************************************************************************/
int main(int argc, char *argv[])
{
	MDIS_PATH	path;
	u_int32     value;
	char	    *device,c;

	printf("Syntax: m76_simp <device>\n");
	printf("Function: M76 example for reading a value\n");
	printf("\n");
	printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	printf("!! For reasons of safety please read the M76 user manual !!\n");
	printf("!! before making measurements using the M-Module!        !!\n");
	printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	printf("\n");
	printf("Options:\n");
	printf("    device       device name\n");
	printf("\n");
	if (argc < 2 || strcmp(argv[1],"-?")==0) {
		return(1);
	}
	do {
		printf("continue (y/n): ");
		
		c = (char)UOS_KeyWait();
		printf ("%c \n",c);
		if (c == 'n')
			return (1);
	} while (c != 'y');

	device = argv[1];

	/*--------------------+
    |  open path          |
    +--------------------*/
	if ((path = M_open(device)) < 0) {
		PrintError("open");
		return(1);
	}

	/*--------------------+
    |  config             |
    +--------------------*/
	/* set current channel */
	if ((M_setstat(path, M_MK_CH_CURRENT, 0)) < 0) {
		PrintError("setstat M_MK_CH_CURRENT");
		goto abort;
	}

	/* set range DC voltage, 12.5V */
	if ((M_setstat(path, M76_RANGE, M76_RANGE_DC_V2)) < 0) {
		PrintError("setstat M76_RANGE");
		goto abort;
	}

	/*--------------------+
    |  read               |
    +--------------------*/
	if ((M_read(path,(int32 *)&value)) < 0) {
		PrintError("read");
		goto abort;
	}

	printf("read: 0x%06x =", (unsigned int)value);
	printf(" ->  %f V\n", (12.5 * 2.0 / FS * (double)value) - 12.5);

	/*--------------------+
    |  clean up           |
    +--------------------*/
	abort:
	if (M_close(path) < 0)
		PrintError("close");

	return(0);
}

/********************************* PrintError *******************************
 *
 *  Description: Print MDIS error message
 *			   
 *---------------------------------------------------------------------------
 *  Input......: info	info string
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/
static void PrintError(char *info)
{
	printf("*** can't %s: %s\n", info, M_errstring(UOS_ErrnoGet()));
}





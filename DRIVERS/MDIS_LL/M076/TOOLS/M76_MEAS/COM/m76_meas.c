/****************************************************************************
 ************                                                    ************
 ************                    M76_MEAS                        ************
 ************                                                    ************
 ****************************************************************************
 *  
 *       Author: ls
 *        $Date: 2010/03/11 17:03:34 $
 *    $Revision: 1.10 $
 *
 *  Description: Configure M76 and perform measurment
 * 
 *
 *     Required: libraries: mdis_api, usr_oss, usr_utl
 *     Switches: -
 *
 *-------------------------------[ History ]---------------------------------
 *
 * $Log: m76_meas.c,v $
 * Revision 1.10  2010/03/11 17:03:34  amorbach
 * R: Porting to MDIS5
 * M: changed according to MDIS Porting Guide 0.8
 *
 * Revision 1.9  2004/04/02 16:39:59  ub
 * removed unneccesary pointer access
 *
 * Revision 1.8  2002/09/10 10:00:52  LSchoberl
 * added test option -1
 *
 * Revision 1.7  2002/09/05 15:19:43  LSchoberl
 * include string.h
 *
 * Revision 1.6  2002/09/04 14:00:59  LSchoberl
 * more test options, default range changed, cosmetics,
 * removed unused function PrintUosError
 *
 * Revision 1.5  2002/08/27 11:47:33  LSchoberl
 * function "usage" changed
 *
 * Revision 1.4  2001/12/18 14:33:08  Schoberl
 * uses defines for range with M76_ prefix
 *
 * Revision 1.3  2001/06/19 16:56:52  Schoberl
 * test checksum & cali info allways
 * print range info
 *
 * Revision 1.2  2001/06/08 10:21:23  kp
 * cosmetics
 *
 * Revision 1.1  2001/06/05 16:24:14  Schoberl
 * Initial Revision
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 2010 by MEN mikro elektronik GmbH, Nuernberg, Germany 
 ****************************************************************************/
 
static const char RCSid[]="$Id: m76_meas.c,v 1.10 2010/03/11 17:03:34 amorbach Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <MEN/men_typs.h>
#include <MEN/usr_oss.h>
#include <MEN/usr_utl.h>
#include <MEN/mdis_api.h>
#include <MEN/m76_drv.h>

/*--------------------------------------+
|   DEFINES                             |
+--------------------------------------*/
#define FS		16777215.0   /* 0x00ffffff */

/* series resistance */
#define RS1		1.0e3
#define RS2		10.0e3
#define RS3		0.1e6
#define RS4		1.0e6

/* protective resistant */
#define RP		100.0		

/* Uconst */
#define UCONST	2.5

/* Im */
#define IM0		( UCONST / (RS1+RP) )
#define IM1		( UCONST / (RS1+RP) )
#define IM2		( UCONST / (RS2+RP) )
#define IM3		( UCONST / (RS3+RP) )
#define IM4		( UCONST / (RS4+RP) )

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
static void usage(void);
static void PrintMdisError(char *info);
static void GetModeString(u_int32 range, char *mode);

/********************************* usage ************************************
 *
 *  Description: Print program usage
 *			   
 *---------------------------------------------------------------------------
 *  Input......: -
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/
static void usage(void)
{
	int32 ch;

page1:
	printf("Usage: m76_meas [<opts>] <device> [<opts>]\n");
	printf("Function: Configure M76 and perform measurement\n");
	printf("\n");
	printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	printf("!! For reasons of safety please read the M76 user manual !!\n");
	printf("!! before making measurements using the M-Module!        !!\n");
	printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	printf("\n");
	printf("Options:\n");
	printf("  device       device name....................... [none]    \n");
	printf("  -n           do not show usage at start.........[no]      \n");
	printf("  -r=<range>   set range ........................ [2]       \n");
	printf("               values and meaning of range see m76_drv.h    \n");
	printf("  -s=<ms>      settling time after channel change [800]     \n");
	printf("  -i           use interrupt waiting for data ... [no]      \n");
	printf("  -l           loop mode......................... [no]      \n");
	printf("  -d=<ms>      delay time for measuring loop .... [1000]    \n");
	printf("                                                            \n");

	printf("continue: 'key'  or number of page(1,2)\n");
	ch = UOS_KeyWait();
	switch (ch)  {
		case'1': printf("\n");goto page1;
		case'2': printf("\n");goto page2;
	}

page2:
	printf("  Test Options:                                             \n");
	printf("  =============                                             \n");
	printf("  -t=<number>  maximal test measuring cycles .... [0]       \n");
	printf("        option -t performs a test that does the following:  \n");
	printf("        - average of <number> measured values (maximal 100000)\n");
	printf("          if key is pressed before <number> measurements  \n");
	printf("          have been performed, average is built from measured \n"); 
	printf("          values until key is pressed.                      \n");
	printf("        - shows min Value.                                  \n");
	printf("        - shows max Value.                                  \n");
	printf("        - errors occuring after path to device is opened    \n");
	printf("          must confirmed.                                   \n");
	printf("  these options are active only when option -t is given:    \n");              
	printf("  -a=<val>    upper valid limit for max Value......... [0.0]\n");
	printf("  -b=<val>    lower valid limit for min Value......... [0.0]\n");
	printf("              with:   min Value >= lower limit                 \n");
	printf("                      max Value <= upper limit                 \n");
	printf("  -1           a message text is output and the.. [no]    \n");
	printf("               program waits for space key is pressed.    \n");
	
	printf("\n");
	printf("(c) 2001 - 2002 by MEN mikro elektronik GmbH\n\n");

	printf("continue: 'key'  or number of page(1,2)\n");
	ch = UOS_KeyWait();
	switch (ch)  {
		case'1': printf("\n");goto page1;
		case'2': printf("\n");goto page2;
	}

}

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
	MDIS_PATH	path=0;
	int32	n,unit=0,readSize,error=0;
	u_int32 range, ints, sTime,value,rVal[2],test,delay,loopmode,usg;
	u_int32 check,message;
	char	*device,*str,*errstr,buf[40],modeStr[40]	;
	double  val=0.0,mb, *valP, *valBuf=NULL, valSum,minLim=0.0,maxLim=0.0;
	double  min=1.7E+308 ,max=-1.7E+308;
	char	c;

	/*--------------------+
    |  check arguments    |
    +--------------------*/
	if ((errstr = UTL_ILLIOPT("a=b=r=s=t=d=iln1?", buf))) {	/* check args */  
		printf("*** %s\n", errstr);
		return(1);
	}

	if (UTL_TSTOPT("?")) {						/* help requested ? */
		usage();
		return(1);
	}

	/*--------------------+
    |  get arguments      |
    +--------------------*/
	for (device=NULL, n=1; n<argc; n++)
		if (*argv[n] != '-') {
			device = argv[n];
			break;
		}

	if (!device) {
		usage();
		return(1);
	}

	range    = ((str = UTL_TSTOPT("r=")) ? atoi(str) : 2);
	sTime    = ((str = UTL_TSTOPT("s=")) ? atoi(str) : 800);
	test     = ((str = UTL_TSTOPT("t=")) ? atoi(str) : 0);
	delay    = ((str = UTL_TSTOPT("d=")) ? atoi(str) : 1000);

	maxLim   = ((str = UTL_TSTOPT("a=")) ? atof(str) : 0.0);
	minLim   = ((str = UTL_TSTOPT("b=")) ? atof(str) : 0.0);
	
	ints     = (UTL_TSTOPT("i") ? 1 : 0);
	loopmode = (UTL_TSTOPT("l") ? 1 : 0);
	usg		 = (UTL_TSTOPT("n") ? 0 : 1);

	message= (UTL_TSTOPT("1") ? 1 : 0);


	if (usg)  {
		usage();
		printf("\n");
		
		do {
			printf("continue (y/n): ");
			
			c = (char)UOS_KeyWait();
			printf ("%c \n",c);
			if (c == 'n')
				return (1);
		} while (c != 'y');
	}

	/* check parameters */
	if (test > 100000)  {
		printf(" too much test cycles\n");
		return(1);
	}

	if (range > M76_RANGE_R4_4)  {
		printf("wrong range\n");
		return(1);
	}

	/* allocate memory if test */
	if (test)  {
		valBuf = malloc((sizeof (double)) * test);
		if( valBuf == NULL )  {
			 printf( "Insufficient memory available\n" );
			 return (1);
		}
		if (minLim >= maxLim)  {
			printf(" you must set max border > min border\n");
			return (1);
		}
	}

	/*--------------------+
    |  open path          |
    +--------------------*/
	if ((path = M_open(device)) < 0) {
		PrintMdisError("open");
		return(1);
	}
	
	/*--------------------+
    |  config             |
    +--------------------*/
	/* set current channel */
	if ((M_setstat(path, M_MK_CH_CURRENT, 0)) < 0) {
		PrintMdisError("setstat M_MK_CH_CURRENT");
		error = 1;
		goto abort;
	}

	/* set interrupt */
	if ((M_setstat(path, M_MK_IRQ_ENABLE, ints)) < 0) {
		PrintMdisError("setstat M_MK_IRQ_ENABLE");
		error = 1;
		goto abort;
	}

	/* set settling Time */
	if ((M_setstat(path, M76_SETTLE, sTime)) < 0) {
		PrintMdisError("setstat M76_SETTLE");
		error = 1;
		goto abort;
	}

	/* set current range */
	if ((M_setstat(path, M76_RANGE, range)) < 0) {
		PrintMdisError("setstat M76_RANGE");
		error = 1;
		goto abort;
	}

    /*--------------------+
    |  print info         |
    +--------------------*/
	/* test checksum */
	if ((M_getstat(path, M76_CHECKSUM, (int32 *)&check)) < 0) {
		PrintMdisError("setstat M76_CHECKSUM");
		error = 1;
		goto abort;
	}
	if (check == TRUE)  {
		printf(" checksum test  : TRUE\n");
	}  
	else  {
		printf(" checksum test  : FALSE\n");
	}

	/* test cali info */
	if ((M_getstat(path, M76_CINFO, (int32 *)&check)) < 0) {
		PrintMdisError("setstat M76_CINFO");
		error = 1;
		goto abort;
	}
	if (check == TRUE)  {
		printf(" cali info      : TRUE\n");
	}  
	else  {
		printf(" cali info      : FALSE\n");
	}
	GetModeString(range, modeStr);
	printf(" measuring range: %u   (%s)\n",(unsigned int)range,modeStr);
	printf(" settling time  : %u\n",(unsigned int)sTime);
	printf(" interrupt used :");
	if (ints)
		printf(" yes\n");
	else
		printf(" no\n");


	/* wait for 'space' if needed */
	if (test)  {
		if (message)  {
			printf("\n--- Wavetek Output ON ---\n\n");
			printf("press SPACE to continue\n");

			while (UOS_KeyWait() != ' ');
		}
	}

	/*--------------------+
    |  read               |
    +--------------------*/
	if (test)  {
		loopmode = test;
	}
	valSum = 0;
	valP = valBuf;
	do {
		/* U and I */
		if (range < M76_RANGE_R2_0)  {
			if ((M_read(path,(int32 *)&value)) < 0) {
				PrintMdisError("read");
				error = 1;
				goto abort;
			}
			printf("read value = 0x%06x\n", (unsigned int)value);
		}
		else  {  /* R */
			/* blockread .... */
			if ((readSize = M_getblock(path, (u_int8*)rVal, 8)) < 0) {
				PrintMdisError("getblock");
				error = 1;
				goto abort;
			}
			else  {
				printf("%d bytes read\n", (int)readSize);		
				printf("read value (1) = 0x%06x        read value (2) = 0x%06x\n",
					    rVal[0], rVal[1]);
			}
			if (rVal[1] == 0)  {
				if (test)  {
					printf("no current, read value (2) = 0 (division by 0!!)\n");
					error = 1;
					goto abort;
				} 
				else  {
					printf("no current, read value (2) = 0 (division by 0!! abort this measure)\n");
					continue;
				}
			}
		}

		/* calculate value*/
		switch (range)  {

		/* dc V */
		case M76_RANGE_DC_V0:
			mb = 125.0e-3;
			val = (mb * 2.0 / FS * (double)value) - mb;
			unit = 1;
			break;
		case M76_RANGE_DC_V1:
			mb = 1.25;
			val = (mb * 2.0 / FS * (double)value) - mb;
			unit = 1;
			break;	
		case M76_RANGE_DC_V2:
			mb = 12.5;
			val = (mb * 2.0 / FS * (double)value) - mb;
			unit = 1;
			break;	
		case M76_RANGE_DC_V3:
			mb = 125.0;
			val = (mb * 2.0 / FS * (double)value) - mb;
			unit = 1;
			break;	
		case M76_RANGE_DC_V4:
			mb = 500.0;
			val = (mb * 2.0 / FS * (double)value) - mb;
			unit = 1;
			break;	

		/* ac V */
		case M76_RANGE_AC_V0:
			val = 250.0e-3 / FS * (double)value;
			unit = 1;
			break;	
		case M76_RANGE_AC_V1:
			val = 2.5 / FS * (double)value;
			unit = 1;
			break;	
		case M76_RANGE_AC_V2:
			val = 25.0 / FS * (double)value;
			unit = 1;
			break;	
		case M76_RANGE_AC_V3:
			val = 250.0 / FS * (double)value;
			unit = 1;
			break;	

		/* dc A */
		case M76_RANGE_DC_A0:
			mb = 12.5e-3;
			val = (mb * 2.0 / FS * (double)value) - mb;
			unit = 2;
			break;	
		case M76_RANGE_DC_A1:
			mb = 125.0e-3;
			val = (mb * 2.0 / FS * (double)value) - mb;
			unit = 2;
			break;	
		case M76_RANGE_DC_A2:
			mb = 1.25;
			val = (mb * 2.0 / FS * (double)value) - mb;
			unit = 2;
			break;	
		case M76_RANGE_DC_A3:
			mb = 2.5;
			val = (mb * 2.0 / FS * (double)value) - mb;
			unit = 2;
			break;	

		/* ac A */
		case M76_RANGE_AC_A0:
			val = 25.0e-3 / FS * (double)value;
			unit = 2;
			break;	
		case M76_RANGE_AC_A1:
			val = 250.0e-3 / FS * (double)value;
			unit = 2;
			break;	
		case M76_RANGE_AC_A2:
			val = 2.5 / FS * (double)value;
			unit = 2;
			break;	
  
		/* 2wire R*/
		case M76_RANGE_R2_0:
			val = ( (UCONST * (double)rVal[0]) / (IM0 * (double)rVal[1]) ) / 4.0;
			unit = 3;
			break;	
		case M76_RANGE_R2_1:
			val =  (UCONST * (double)rVal[0]) / (IM1 * (double)rVal[1]);
			unit = 3;
			break;	
		case M76_RANGE_R2_2:
			val =  (UCONST * (double)rVal[0]) / (IM2 * (double)rVal[1]);
			unit = 3;
			break;	
		case M76_RANGE_R2_3:
			val =  (UCONST * (double)rVal[0]) / (IM3 * (double)rVal[1]);
			unit = 3;
			break;	
		case M76_RANGE_R2_4:
			val =  (UCONST * (double)rVal[0]) / (IM4 * (double)rVal[1]);
			unit = 3;
			break;	

		/* 4wire R */
		case M76_RANGE_R4_0:
			val =  (UCONST * (double)rVal[0]) / (IM0 * (double)rVal[1]) / 4.0 ;
			unit = 3;
			break;	
		case M76_RANGE_R4_1:
			val =  (UCONST * (double)rVal[0]) / (IM1 * (double)rVal[1]);
			unit = 3;
			break;	
		case M76_RANGE_R4_2:
			val =  (UCONST * (double)rVal[0]) / (IM2 * (double)rVal[1]);
			unit = 3;
			break;	
		case M76_RANGE_R4_3:
			val =  (UCONST * (double)rVal[0]) / (IM3 * (double)rVal[1]);
			unit = 3;
			break;	
		case M76_RANGE_R4_4:
			val =  (UCONST * (double)rVal[0]) / (IM4 * (double)rVal[1]);
			unit = 3;
			break;	
		}

		if (test)  {
			valSum += val;
			*valP++ = val;
			loopmode--;
			if (val < min)
				min = val;
			if (val > max)
				max = val;
		}
			
		printf(" %f",val);
		if (unit == 1)
			printf("V");
		if (unit == 2)
			printf("A");
		if (unit == 3)
			printf("Ohm");
		printf("  (%s) ",modeStr);
		if (test)
			printf("    average Sum = %f\n", valSum);
		else
			printf("\n");
		printf(" \n");

		UOS_Delay(delay);
	} while (loopmode && UOS_KeyPressed() == -1);

	if (test) {
		double average;
		u_int32 minCnt=0,maxCnt=0,hitCnt=0,nbr;

		average = valSum/((double)test-(double)loopmode);
		nbr = test-loopmode;
		valP = valBuf;

		/* get values < or > average */
		while( loopmode++ < test )  {  
			if (*valP < average)						
				minCnt++; 
			if (*valP > average)
				maxCnt++;
			if (*valP == average)
				hitCnt++;

            valP++;
		}

		printf("+------------------------------------------------------------\n");
		printf("|                      TEST RESULTS                          \n");
		printf("+------------------------------------------------------------\n");
		printf("| number of measurements: %u\n",(unsigned int)nbr );                
		printf("| average = %.15le\n", average);
		printf("| min Value = %.15le\n", min);
		printf("| max Value = %.15le\n", max);
		printf("| value < average: %u\n", (unsigned int)minCnt);
		printf("| value = average: %u\n", (unsigned int)hitCnt);
		printf("| value > average: %u\n", (unsigned int)maxCnt);
		printf("+------------------------------------------------------------\n");

		/* error check */
		if ( (min < minLim) || (max > maxLim) )  {
			error = 1;
			printf("\n\n");
			printf("    +-------------------------------------------------------+\n");
			printf("    |+-----------------------------------------------------+|\n");
			printf("    ||                      E R R O R                      ||\n");
			printf("    ||      min Value or max Value out of given limits     ||\n");
			printf("    |+-----------------------------------------------------+|\n");
			printf("    +-------------------------------------------------------+\n");
			printf("(lower valid limit %f)\n", minLim);
			printf("(upper valid limit %f)\n", maxLim);
		}
	}

	/*--------------------+
    |  cleanup            |
    +--------------------*/
	abort:
	if (test)
		free(valBuf);
	if (M_close(path) < 0)  {
		PrintMdisError("close");
		error = 1;
	}

	if (error && test)  {
		printf("\nerror occured\n");
		printf("==> press 'e' to continue\n");
		while (UOS_KeyWait() != 'e')
			;
	}
	return(error);
}

/********************************* PrintMdisError ***************************
 *
 *  Description: Print MDIS error message
 *			   
 *---------------------------------------------------------------------------
 *  Input......: info	info string
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/
static void PrintMdisError(char *info)
{
	printf("*** can't %s: %s\n", info, M_errstring(UOS_ErrnoGet()));
}

/********************************* GetModeString ****************************
 *
 *  Description: Get String for Range
 *			   
 *---------------------------------------------------------------------------
 *  Input......: info	info string
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/
static void GetModeString(u_int32 range, char *mode)
{
	switch (range)  {
	case M76_RANGE_DC_V0: strcpy(mode,"DC voltage, 125mV"); break;			/* 0 */ 
	case M76_RANGE_DC_V1: strcpy(mode,"DC voltage, 1.25V"); break;			/* 1 */ 
	case M76_RANGE_DC_V2: strcpy(mode,"DC voltage, 12.5V"); break;			/* 2 */ 
	case M76_RANGE_DC_V3: strcpy(mode,"DC voltage, 125V"); break;			/* 3 */ 
	case M76_RANGE_DC_V4: strcpy(mode,"DC voltage, 500V"); break;			/* 4 */ 

	case M76_RANGE_AC_V0: strcpy(mode,"AC voltage, 250mV"); break;			/* 5 */ 
	case M76_RANGE_AC_V1: strcpy(mode,"AC voltage, 2.5V"); break;			/* 6 */ 
	case M76_RANGE_AC_V2: strcpy(mode,"AC voltage, 25V"); break;			/* 7 */ 
	case M76_RANGE_AC_V3: strcpy(mode,"AC voltage, 250V"); break;			/* 8 */ 

	case M76_RANGE_DC_A0: strcpy(mode,"DC current, 12.5mA"); break;			/* 9 */ 
	case M76_RANGE_DC_A1: strcpy(mode,"DC current, 125mA"); break;			/* 10 */ 
	case M76_RANGE_DC_A2: strcpy(mode,"DC current, 1.25A"); break;			/* 11 */ 
	case M76_RANGE_DC_A3: strcpy(mode,"DC current, 2.5A"); break;			/* 12 */ 

	case M76_RANGE_AC_A0: strcpy(mode,"AC current, 25mA"); break;			/* 13 */ 
	case M76_RANGE_AC_A1: strcpy(mode,"AC current, 250mA"); break;			/* 14 */ 
	case M76_RANGE_AC_A2: strcpy(mode,"AC current, 2.5A"); break;			/* 15 */ 
  
	case M76_RANGE_R2_0	: strcpy(mode,"resistance, 2-wire, 250 OHM"); break; /* 16 */ 
	case M76_RANGE_R2_1	: strcpy(mode,"resistance, 2-wire, 2.5 kOHM"); break;/* 17 */ 
	case M76_RANGE_R2_2	: strcpy(mode,"resistance, 2-wire, 25 kOHM"); break; /* 18 */ 
	case M76_RANGE_R2_3	: strcpy(mode,"resistance, 2-wire, 250 kOHM"); break;/* 19 */ 
	case M76_RANGE_R2_4	: strcpy(mode,"resistance, 2-wire, 2.5 MOHM"); break;/* 20 */ 
 
	case M76_RANGE_R4_0	: strcpy(mode,"resistance, 4-wire, 250 OHM"); break; /* 21 */ 
	case M76_RANGE_R4_1	: strcpy(mode,"resistance, 4-wire, 2.5 kOHM"); break;/* 22 */ 
	case M76_RANGE_R4_2	: strcpy(mode,"resistance, 4-wire, 25 kOHM"); break; /* 23 */ 
	case M76_RANGE_R4_3	: strcpy(mode,"resistance, 4-wire, 250 kOHM"); break;/* 24 */ 
	case M76_RANGE_R4_4	: strcpy(mode,"resistance, 4-wire, 2.5 MOHM"); break;/* 25 */ 
	default:          strcpy(mode,"invalid range"); break; 

	}
}








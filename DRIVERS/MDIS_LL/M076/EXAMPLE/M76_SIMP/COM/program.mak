#***************************  M a k e f i l e  *******************************
#
#         Author: ls
#          $Date: 2004/04/02 16:40:03 $
#      $Revision: 1.2 $
#
#    Description: Makefile definitions for the M76 example program
#
#---------------------------------[ History ]---------------------------------
#
#   $Log: program.mak,v $
#   Revision 1.2  2004/04/02 16:40:03  ub
#   removed mdis_err.h from include list
#
#   Revision 1.1  2001/06/05 16:24:18  Schoberl
#   Initial Revision
#
#-----------------------------------------------------------------------------
#   (c) Copyright 2001 by MEN mikro elektronik GmbH, Nuernberg, Germany
#*****************************************************************************

MAK_NAME=m76_simp

MAK_LIBS=$(LIB_PREFIX)$(MEN_LIB_DIR)/mdis_api$(LIB_SUFFIX)	\
		 $(LIB_PREFIX)$(MEN_LIB_DIR)/usr_oss$(LIB_SUFFIX)

MAK_INCL=$(MEN_INC_DIR)/m76_drv.h	\
         $(MEN_INC_DIR)/men_typs.h	\
         $(MEN_INC_DIR)/mdis_api.h	\
         $(MEN_INC_DIR)/usr_oss.h	\

MAK_INP1=m76_simp$(INP_SUFFIX)

MAK_INP=$(MAK_INP1)

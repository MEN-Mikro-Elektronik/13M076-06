#************************** MDIS5 device descriptor *************************
#
#        Author: ls
#         $Date: 2010/03/11 17:04:59 $
#     $Revision: 1.2 $
#
#   Description: Metadescriptor for M76 (swapped variant)
#
#****************************************************************************

M76_SW_1  {
	#------------------------------------------------------------------------
	#	general parameters (don't modify)
	#------------------------------------------------------------------------
    DESC_TYPE        = U_INT32  1           # descriptor type (1=device)
    HW_TYPE          = STRING   M076_SW     # hardware name of device

	#------------------------------------------------------------------------
	#	reference to base board
	#------------------------------------------------------------------------
    BOARD_NAME       = STRING   D201_1      # device name of baseboard
    DEVICE_SLOT      = U_INT32  0           # used slot on baseboard (0..n)

	#------------------------------------------------------------------------
	#	device parameters
	#------------------------------------------------------------------------
}

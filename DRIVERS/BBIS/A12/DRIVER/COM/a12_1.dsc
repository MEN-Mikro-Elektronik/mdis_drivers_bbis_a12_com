#************************** BBIS3 board descriptor **************************
#
#        Author: kp
#         $Date: 2001/04/11 12:26:11 $
#     $Revision: 1.1 $
#
#   Description: Metadescriptor for A12
#
#****************************************************************************

A12 {
	#------------------------------------------------------------------------
	#	general parameters (don't modify)
	#------------------------------------------------------------------------
    DESC_TYPE           = U_INT32  2           # descriptor type (2=board)
    HW_TYPE             = STRING   A12         # hardware name of device

        DEBUG_LEVEL         = U_INT32    0xc0008007  # BB driver
	    DEBUG_LEVEL_BK      = U_INT32    0xc0008007  # BBIS kernel
    	DEBUG_LEVEL_OSS     = U_INT32    0xc0008000  # OSS calls
	    DEBUG_LEVEL_DESC    = U_INT32    0xc0008000  # DESC calls
}

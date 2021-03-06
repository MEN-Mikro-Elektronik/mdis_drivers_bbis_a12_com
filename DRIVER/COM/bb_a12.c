/*********************  P r o g r a m  -  M o d u l e ***********************
 *
 *         Name: bb_a12.c
 *      Project: A12 board handler
 *
 *       Author: kp
 *
 *  Description: A12 board handler
 *
 *               The A12 supports several onboard-devices:
 *
 *				 device
 *               -----------------------------------------------------------
 *               0x0	 M-Module 0
 *               0x1	 M-Module 1
 *               0x2	 M-Module 2
 *				 0x1000	 QSPI
 *
 *     			 The onboard PC-MIP and PMC slots are handled by the
 *				 generic PCI BBIS
 *
 *     Note: do not compile this handler with MAC_BYTESWAP!
 *
 *     Required: ---
 *     Switches: _ONE_NAMESPACE_PER_DRIVER_
 *
 *---------------------------------------------------------------------------
 * Copyright 2001-2019, MEN Mikro Elektronik GmbH
 ******************************************************************************/
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#define _NO_BBIS_HANDLE		/* bb_defs.h: don't define BBIS_HANDLE struct */

#include <MEN/mdis_com.h>
#include <MEN/men_typs.h>   /* system dependend definitions   */
#include <MEN/dbg.h>        /* debug functions                */
#include <MEN/oss.h>        /* oss functions                  */
#include <MEN/desc.h>       /* descriptor functions           */
#include <MEN/bb_defs.h>    /* bbis definitions				  */
#include <MEN/mdis_err.h>   /* MDIS error codes               */
#include <MEN/mdis_api.h>   /* MDIS global defs               */
#include <MEN/maccess.h>

#include "a12_int.h"		/* A12 specific defines */

/*-----------------------------------------+
|  DEFINES                                 |
+-----------------------------------------*/
/* debug settings */
#define DBG_MYLEVEL		h->debugLevel
#define DBH             h->debugHdl


#define CFIDX(i) ((i)<BBIS_SLOTS_ONBOARDDEVICE_START ? (i):((i)-BBIS_SLOTS_ONBOARDDEVICE_START+3))

/* include files which need BBIS_HANDLE */
#include <MEN/bb_entry.h>	/* bbis jumptable				  */
#include <MEN/bb_a12.h>		/* A12 bbis header file			  */

static const char IdentString[]=MENT_XSTR(MAK_REVISION);

typedef struct {
	u_int32 devBusType;
	u_int32 interrupts;
	u_int32 addrSpace;
	int32 	pciBusNbr;
	int32 	pciDevNbr;
	int32   irqLevel;
	int32   irqMode;
} A12_SLOT_CFG;

/*-----------------------------------------+
|  GLOBALS                                 |
+-----------------------------------------*/
const A12_SLOT_CFG G_slotCfg[BRD_NBR_OF_BRDDEV] = {
	/* M-mods */
	{ OSS_BUSTYPE_MMODULE, BBIS_IRQ_DEVIRQ, OSS_ADDRSPACE_MEM, -1, -1, -1,
	  BBIS_IRQ_SHARED },
	{ OSS_BUSTYPE_MMODULE, BBIS_IRQ_DEVIRQ, OSS_ADDRSPACE_MEM, -1, -1, -1,
	  BBIS_IRQ_SHARED },
	{ OSS_BUSTYPE_MMODULE, BBIS_IRQ_DEVIRQ, OSS_ADDRSPACE_MEM, -1, -1, -1,
	  BBIS_IRQ_SHARED },
	/* QSPI */
	{ OSS_BUSTYPE_NONE, BBIS_IRQ_DEVIRQ, OSS_ADDRSPACE_MEM, -1, -1, 9,
	  BBIS_IRQ_EXCLUSIVE }
};


/*-----------------------------------------+
|  PROTOTYPES                              |
+-----------------------------------------*/
/* init/exit */
static int32 A12_Init(OSS_HANDLE*, DESC_SPEC*, BBIS_HANDLE**);
static int32 A12_BrdInit(BBIS_HANDLE*);
static int32 A12_BrdExit(BBIS_HANDLE*);
static int32 A12_Exit(BBIS_HANDLE**);
/* info */
static int32 A12_BrdInfo(u_int32, ...);
static int32 A12_CfgInfo(BBIS_HANDLE*, u_int32, ...);
/* interrupt handling */
static int32 A12_IrqEnable(BBIS_HANDLE*, u_int32, u_int32);
static int32 A12_IrqSrvInit(BBIS_HANDLE*, u_int32);
static void  A12_IrqSrvExit(BBIS_HANDLE*, u_int32);
/* exception handling */
static int32 A12_ExpEnable(BBIS_HANDLE*,u_int32, u_int32);
static int32 A12_ExpSrv(BBIS_HANDLE*,u_int32);
/* get module address */
static int32 A12_SetMIface(BBIS_HANDLE*, u_int32, u_int32, u_int32);
static int32 A12_ClrMIface(BBIS_HANDLE*,u_int32);
static int32 A12_GetMAddr(BBIS_HANDLE*, u_int32, u_int32, u_int32, void**, u_int32*);
/* getstat/setstat */
static int32 A12_SetStat(BBIS_HANDLE*, u_int32, int32, INT32_OR_64);
static int32 A12_GetStat(BBIS_HANDLE*, u_int32, int32, INT32_OR_64*);
/* unused */
static int32 A12_Unused(void);
/* miscellaneous */
static char* Ident( void );
static int32 Cleanup(BBIS_HANDLE *h, int32 retCode);


/**************************** A12_GetEntry ***********************************
 *
 *  Description:  Initialize drivers jump table.
 *
 *---------------------------------------------------------------------------
 *  Input......:  bbisP     pointer to the not initialized structure
 *  Output.....:  *bbisP    initialized structure
 *  Globals....:  ---
 ****************************************************************************/
#ifdef _ONE_NAMESPACE_PER_DRIVER_
	extern void BBIS_GetEntry( BBIS_ENTRY *bbisP )
#else
	extern void A12_GetEntry( BBIS_ENTRY *bbisP )
#endif
{
    /* init/exit */
    bbisP->init         =   A12_Init;
    bbisP->brdInit      =   A12_BrdInit;
    bbisP->brdExit      =   A12_BrdExit;
    bbisP->exit         =   A12_Exit;
    bbisP->fkt04        =   A12_Unused;
    /* info */
    bbisP->brdInfo      =   A12_BrdInfo;
    bbisP->cfgInfo      =   A12_CfgInfo;
    bbisP->fkt07        =   A12_Unused;
    bbisP->fkt08        =   A12_Unused;
    bbisP->fkt09        =   A12_Unused;
    /* interrupt handling */
    bbisP->irqEnable    =   A12_IrqEnable;
    bbisP->irqSrvInit   =   A12_IrqSrvInit;
    bbisP->irqSrvExit   =   A12_IrqSrvExit;
    bbisP->setIrqHandle =   NULL;
    bbisP->fkt14        =   A12_Unused;
    /* exception handling */
    bbisP->expEnable    =   A12_ExpEnable;
    bbisP->expSrv       =   A12_ExpSrv;
    bbisP->fkt17        =   A12_Unused;
    bbisP->fkt18        =   A12_Unused;
    bbisP->fkt19        =   A12_Unused;
    /* */
    bbisP->fkt20        =   A12_Unused;
    bbisP->fkt21        =   A12_Unused;
    bbisP->fkt22        =   A12_Unused;
    bbisP->fkt23        =   A12_Unused;
    bbisP->fkt24        =   A12_Unused;
    /*  getstat / setstat / address setting */
    bbisP->setStat      =   A12_SetStat;
    bbisP->getStat      =   A12_GetStat;
    bbisP->setMIface    =   A12_SetMIface;
    bbisP->clrMIface    =   A12_ClrMIface;
    bbisP->getMAddr     =   A12_GetMAddr;
    bbisP->fkt30        =   A12_Unused;
    bbisP->fkt31        =   A12_Unused;
}

/****************************** A12_Init *************************************
 *
 *  Description:  Allocate and return board handle.
 *
 *                - initializes the board handle
 *                - reads and saves board descriptor entries
 *                - check for M-module FPGA
 *				  - Locate memory base
 *				  - Assign resources to board handler
 *				  - Map used resources
 *
 *                The following descriptor keys are used:
 *
 *                Deskriptor key           Default          Range
 *                -----------------------  ---------------  -------------
 *                DEBUG_LEVEL_DESC         OSS_DBG_DEFAULT  see dbg.h
 *                DEBUG_LEVEL              OSS_DBG_DEFAULT  see dbg.h
 *---------------------------------------------------------------------------
 *  Input......:  osHdl     pointer to os specific structure
 *                descSpec  pointer to os specific descriptor specifier
 *                hP   pointer to not initialized board handle structure
 *  Output.....:  *hP  initialized board handle structure
 *				  return    0 | error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 A12_Init(
    OSS_HANDLE      *osHdl,
    DESC_SPEC       *descSpec,
    BBIS_HANDLE     **hP )
{
    BBIS_HANDLE	*h = NULL;
	u_int32     gotsize;
    u_int32		value;
	int32 venId, devId, error, i;

    /*-------------------------------+
    | initialize the board structure |
    +-------------------------------*/
    /* get memory for the board structure */
    *hP = h = (BBIS_HANDLE*) (OSS_MemGet(
        osHdl, sizeof(BBIS_HANDLE), &gotsize ));
    if ( h == NULL )
        return ERR_OSS_MEM_ALLOC;

    /* store data into the board structure */
    h->ownMemSize = gotsize;
    h->osHdl = osHdl;

    /*------------------------------+
    |  init id function table       |
    +------------------------------*/
	/* drivers ident function */
	h->idFuncTbl.idCall[0].identCall = Ident;
	/* libraries ident functions */
	h->idFuncTbl.idCall[1].identCall = DESC_Ident;
	h->idFuncTbl.idCall[2].identCall = OSS_Ident;
	/* terminator */
	h->idFuncTbl.idCall[3].identCall = NULL;

    /*------------------------------+
    |  prepare debugging            |
    +------------------------------*/
	DBG_MYLEVEL = OSS_DBG_DEFAULT;	/* set OS specific debug level */
	DBGINIT((NULL,&DBH));

    DBGWRT_1((DBH,"BB - %s_Init\n",BBNAME));

    /*------------------------------+
    |  scan descriptor              |
    +------------------------------*/
    /* init descHdl */
    error = DESC_Init( descSpec, osHdl, &h->descHdl );
	if (error)
		return( Cleanup(h,error) );

    /* get DEBUG_LEVEL_DESC */
    error = DESC_GetUInt32(h->descHdl, OSS_DBG_DEFAULT, &value,
				"DEBUG_LEVEL_DESC");
	if ( error && (error!=ERR_DESC_KEY_NOTFOUND) )
		return( Cleanup(h,error) );

	/* set debug level for DESC module */
	DESC_DbgLevelSet(h->descHdl, value);

    /* get DEBUG_LEVEL */
    error = DESC_GetUInt32( h->descHdl, OSS_DBG_DEFAULT,
							 &(h->debugLevel),
                "DEBUG_LEVEL");
	if ( error && (error!=ERR_DESC_KEY_NOTFOUND) )
		return( Cleanup(h,error) );


	/*-----------------------------------+
	|  Check if M-module bridge present  |
	+-----------------------------------*/
	error = OSS_PciGetConfig( h->osHdl, 0, A12_MMOD_BRIDGE_DEV_NO, 0,
							  OSS_PCI_VENDOR_ID, &venId );
	error = OSS_PciGetConfig( h->osHdl, 0, A12_MMOD_BRIDGE_DEV_NO, 0,
							  OSS_PCI_DEVICE_ID, &devId );
	if( error ){
		DBGWRT_ERR((DBH, "*** %s_BrdInit: Can't read PCI ven/dev Id\n",
					BBNAME ));
		return( Cleanup(h,error) );
	}

	if( venId == 0xffff && devId == 0xffff ){
		DBGWRT_ERR((DBH, "*** %s_BrdInit: PCI->M-mod bridge not present!\n",
					BBNAME));
		return( Cleanup(h,ERR_BBIS_ILL_ID));
	}

	if( (venId != A12_MMOD_BRIDGE_VEN_ID) ||
		(devId != A12_MMOD_BRIDGE_DEV_ID )){
		DBGWRT_ERR((DBH, "*** %s_BrdInit: bad vendor/device ID of "
					"PCI->M-mod bridge %04lx %04lx\n", BBNAME, venId, devId));
		return( Cleanup(h,ERR_BBIS_ILL_ID));
	}

	/*------------------------------------------+
	|  Determine base address of M-module regs  |
	+------------------------------------------*/
	error = OSS_BusToPhysAddr( h->osHdl, OSS_BUSTYPE_PCI, &h->physBase,
							   0, A12_MMOD_BRIDGE_DEV_NO, 0, 0 ); /* BAR0 */
	if( error ){
		DBGWRT_ERR((DBH, "*** %s_BrdInit: Can't read BAR0 Id\n",
					BBNAME ));
		return( Cleanup(h,error) );
	}
	DBGWRT_2((DBH," physBase 0x%08lx\n", h->physBase ));

	/*------------------------------------+
	|  Assign resources for control regs  |
	+------------------------------------*/
	for( i=0; i<A12_NBR_OF_MMODS; i++ ){
		h->res[i].type = OSS_RES_MEM;
		h->res[i].u.mem.physAddr = (void *)((INT32_OR_64)h->physBase +
			(A12_MMOD_SLOT_OFFSET*i) + A12_MMOD_CTRL_BASE);
		h->res[i].u.mem.size = A12_CTRL_SIZE;
	}

    /* assign the resources */
    if( (error = OSS_AssignResources( h->osHdl, OSS_BUSTYPE_PCI, 0,
                                      A12_NBR_OF_MMODS, h->res )) )
		return Cleanup(h, error);

	h->resourcesAssigned = TRUE;
	/*---------------------+
	|  Map used resources  |
	+---------------------*/
	for( i=0; i<A12_NBR_OF_MMODS; i++ ){
		error = OSS_MapPhysToVirtAddr(
				h->osHdl,
				(void*)( (INT32_OR_64)h->physBase +
						 (A12_MMOD_SLOT_OFFSET*i) + A12_MMOD_CTRL_BASE),
				A12_CTRL_SIZE,
				OSS_ADDRSPACE_MEM,
				OSS_BUSTYPE_PCI,
				0,
				(void *)&h->mmod[i].vCtrlBase );
		if( error ) return Cleanup( h, error );
		DBGWRT_2((DBH," vCtrlBase for M-mod %d: 0x%08lx\n", i,
				  h->mmod[i].vCtrlBase));
	}

    /* get interrupt line */
	error = OSS_PciGetConfig( h->osHdl, 0, A12_MMOD_BRIDGE_DEV_NO, 0,
							   OSS_PCI_INTERRUPT_LINE, &h->irqLevel );

	/* no interrupt connected */
	if( error || (h->irqLevel == 0xff) )
		return Cleanup(h,ERR_BBIS_NO_IRQ);

	/* convert level to vector */
	if( (error = OSS_IrqLevelToVector( h->osHdl, OSS_BUSTYPE_PCI,
										h->irqLevel, &h->irqVector )) )
		return Cleanup(h,error);

	DBGWRT_2((DBH," IRQ level=0x%x, vector=0x%x\n",
			  h->irqLevel, h->irqVector));


    return 0;
}

/****************************** A12_BrdInit **********************************
 *
 *  Description:  Board initialization.
 *				  - init all control regs to a safe state
 *---------------------------------------------------------------------------
 *  Input......:  h    		pointer to board handle structure
 *  Output.....:  return    0 | error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 A12_BrdInit(
    BBIS_HANDLE     *h )
{
	int32 mSlot;

	DBGWRT_1((DBH, "BB - %s_BrdInit\n",BBNAME));

	for( mSlot=0; mSlot<A12_NBR_OF_MMODS; mSlot++ ){
		MWRITE_D8( h->mmod[mSlot].vCtrlBase, 0, 0xc );	/* fast bit set */
	}

	return 0;
}

/****************************** A12_BrdExit **********************************
 *
 *  Description:  Board deinitialization.
 *
 *                - init all control regs to a safe state
 *
 *---------------------------------------------------------------------------
 *  Input......:  h    pointer to board handle structure
 *  Output.....:  return    0 | error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 A12_BrdExit(
    BBIS_HANDLE     *h )
{
	int32 mSlot;

	DBGWRT_1((DBH, "BB - %s_BrdExit\n",BBNAME));

	for( mSlot=0; mSlot<A12_NBR_OF_MMODS; mSlot++ ){
		MWRITE_D8( h->mmod[mSlot].vCtrlBase, 0, 0xc );	/* fast bit set */
	}

    return 0;
}

/****************************** A12_Exit *************************************
 *
 *  Description:  Cleanup memory.
 *
 *                - deinitializes the bbis handle
 *
 *---------------------------------------------------------------------------
 *  Input......:  hP   pointer to board handle structure
 *  Output.....:  *hP  NULL
 *                return    0 | error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 A12_Exit(
    BBIS_HANDLE     **hP )
{
    BBIS_HANDLE	*h = *hP;
	int32		error = 0;

    DBGWRT_1((DBH, "BB - %s_Exit\n",BBNAME));

    /*------------------------------+
    |  cleanup memory               |
    +------------------------------*/
	error = Cleanup(h, error);
    *hP = NULL;

    return error;
}

/****************************** A12_BrdInfo **********************************
 *
 *  Description:  Get information about hardware and driver requirements.
 *
 *                Following info codes are supported:
 *
 *                Code                      Description
 *                ------------------------  -----------------------------
 *                BBIS_BRDINFO_BUSTYPE      board bustype
 *                BBIS_BRDINFO_DEVBUSTYPE   device bustype
 *                BBIS_BRDINFO_FUNCTION     used optional functions
 *                BBIS_BRDINFO_NUM_SLOTS    number of slots
 *				  BBIS_BRDINFO_INTERRUPTS   interrupt characteristics
 *                BBIS_BRDINFO_ADDRSPACE    address characteristic
 *
 *                The BBIS_BRDINFO_BUSTYPE code returns the bustype of
 *                the specified device - not the board bus type.
 *
 *                The BBIS_BRDINFO_FUNCTION code returns the information
 *                if an optional BBIS function is supported or not.
 *
 *                The BBIS_BRDINFO_NUM_SLOTS code returns the number of
 *                devices used from the driver.
 *
 *                The BBIS_BRDINFO_INTERRUPTS code returns the supported
 *                interrupt capability (BBIS_IRQ_DEVIRQ/BBIS_IRQ_EXPIRQ)
 *                of the specified device.
 *
 *                The BBIS_BRDINFO_ADDRSPACE code returns the address
 *                characteristic (OSS_ADDRSPACE_MEM/OSS_ADDRSPACE_IO)
 *                of the specified device.
 *
 *---------------------------------------------------------------------------
 *  Input......:  code      reference to the information we need
 *                ...       variable arguments
 *  Output.....:  *...      variable arguments
 *                return    0 | error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 A12_BrdInfo(
    u_int32 code,
    ... )
{
	int32		status = ERR_SUCCESS;
    va_list     argptr;

    va_start(argptr,code);

    switch ( code ) {

    /* supported functions */
	case BBIS_BRDINFO_FUNCTION:
        {
        u_int32 funcCode = va_arg( argptr, u_int32 );
        u_int32 *used = va_arg( argptr, u_int32* );

		switch( funcCode )
		{
			/* supported */
			case BBIS_FUNC_IRQENABLE:
			case BBIS_FUNC_IRQSRVINIT:
				*used = TRUE;
				break;
			/* unsupported */
			default:
				*used = FALSE;
		}

        break;
        }

    /* total number of devices */
	case BBIS_BRDINFO_NUM_SLOTS:
	{
		u_int32 *numSlot = va_arg( argptr, u_int32* );

		*numSlot = BRD_NBR_OF_BRDDEV;
		break;
	}

	/* bus type */
	case BBIS_BRDINFO_BUSTYPE:
	{
		u_int32 *busType = va_arg( argptr, u_int32* );

		*busType = OSS_BUSTYPE_PCI;
		break;
	}

	/* device bus type */
	case BBIS_BRDINFO_DEVBUSTYPE:
	{
		u_int32 mSlot       = va_arg( argptr, u_int32 );
		u_int32 *devBusType = va_arg( argptr, u_int32* );

		*devBusType = G_slotCfg[CFIDX(mSlot)].devBusType;
		break;
	}

	/* interrupt capability */
	case BBIS_BRDINFO_INTERRUPTS:
	{
		u_int32 mSlot = va_arg( argptr, u_int32 );
		u_int32 *irqP = va_arg( argptr, u_int32* );

		*irqP = G_slotCfg[CFIDX(mSlot)].interrupts;
		break;
	}

	/* address space type */
	case BBIS_BRDINFO_ADDRSPACE:
	{
		u_int32 mSlot      = va_arg( argptr, u_int32 );
		u_int32 *addrSpace = va_arg( argptr, u_int32* );

		*addrSpace = G_slotCfg[CFIDX(mSlot)].addrSpace;
		break;
	}

	/* error */
	default:
		status = ERR_BBIS_UNK_CODE;
    }

    va_end( argptr );
    return status;
}

/****************************** A12_CfgInfo **********************************
 *
 *  Description:  Get information about board configuration.
 *
 *                Following info codes are supported:
 *
 *                Code                      Description
 *                ------------------------  ------------------------------
 *                BBIS_CFGINFO_BUSNBR       bus number
 *				  BBIS_CFGINFO_PCI_DEVNBR	PCI device number
 *                BBIS_CFGINFO_IRQ          interrupt parameters
 *                BBIS_CFGINFO_EXP          exception interrupt parameters
 *
 *                The BBIS_CFGINFO_BUSNBR code returns the number of the
 *                bus on which the specified device resides
 *
 *                The BBIS_CFGINFO_PCI_DEVNBR code returns the device number
 *                on the PCI bus on which the specified device resides
 *
 *                The BBIS_CFGINFO_IRQ code returns the device interrupt
 *                vector, level and mode of the specified device.
 *
 *                The BBIS_CFGINFO_EXP code returns the exception interrupt
 *                vector, level and mode of the specified device.
 *
 *---------------------------------------------------------------------------
 *  Input......:  h    pointer to board handle structure
 *                code      reference to the information we need
 *                ...       variable arguments
 *  Output.....:  ...       variable arguments
 *                return    0 | error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 A12_CfgInfo(
    BBIS_HANDLE     *h,
    u_int32         code,
    ... )
{
    va_list		argptr;
    int32       status=0;


    DBGWRT_1((DBH, "BB - %s_CfgInfo\n",BBNAME));

    va_start(argptr,code);

    switch ( code ) {

    /* bus number */
	case BBIS_CFGINFO_BUSNBR:
	{
		u_int32 *busNbr = va_arg( argptr, u_int32* );
		u_int32 mSlot   = va_arg( argptr, u_int32 );

		if ( G_slotCfg[CFIDX(mSlot)].pciBusNbr >= 0 )
			*busNbr = G_slotCfg[CFIDX(mSlot)].pciBusNbr;
		else
			*busNbr = 0;		/* M-module bridge on bus 0 */

		break;
	}

	/* PCI device number */
	case BBIS_CFGINFO_PCI_DEVNBR:
	{
		u_int32 mSlot      = va_arg( argptr, u_int32 );
		u_int32 *pciDevNbr = va_arg( argptr, u_int32* );

		if ( G_slotCfg[CFIDX(mSlot)].pciDevNbr >= 0 )
			*pciDevNbr = G_slotCfg[CFIDX(mSlot)].pciDevNbr;
		else
			*pciDevNbr = A12_MMOD_BRIDGE_DEV_NO;
		break;
	}

	/* interrupt information */
	case BBIS_CFGINFO_IRQ:
	{
		int32 mSlot   = va_arg( argptr, int32 );
		u_int32 *vector = va_arg( argptr, u_int32* );
		u_int32 *level  = va_arg( argptr, u_int32* );
		u_int32 *mode   = va_arg( argptr, u_int32* );

		/* check device number */
		if( (CFIDX(mSlot) < 0) || (CFIDX(mSlot) >= BRD_NBR_OF_BRDDEV )){
			DBGWRT_ERR((DBH,"*** %s_CfgInfo: mSlot=0x%x not supported\n",
						BBNAME,mSlot));
			va_end( argptr );
			return ERR_BBIS_ILL_SLOT;
		}

		*mode  = G_slotCfg[CFIDX(mSlot)].irqMode;

		if( G_slotCfg[CFIDX(mSlot)].irqLevel == -1 ){
			*level = h->irqLevel;
			*vector = h->irqVector;
		}
		else {
			*level = G_slotCfg[CFIDX(mSlot)].irqLevel;

			/* convert level to vector */
			status = OSS_IrqLevelToVector(
						h->osHdl,
						G_slotCfg[CFIDX(mSlot)].devBusType,
						(int32)*level,
						(int32*)vector );
		}
		DBGWRT_2((DBH, " dev:%d : IRQ mode=0x%x, level=0x%x, vector=0x%x\n",
				mSlot, *mode, *level, *vector));

		if (status) {
			va_end( argptr );
			return status;
		}
		break;
	}

	/* exception interrupt information */
	case BBIS_CFGINFO_EXP:
	{
		u_int32 mSlot   = va_arg( argptr, u_int32 );
		u_int32 *vector = va_arg( argptr, u_int32* );
		u_int32 *level  = va_arg( argptr, u_int32* );
		u_int32 *mode   = va_arg( argptr, u_int32* );
		u_int32 *dummy;
		u_int32 dummy2;

		dummy = vector;
		dummy = level;
		dummy2 = mSlot;
		/* no extra exception interrupt */
		*mode = 0;
		break;
	}


	/* error */
	default:
		DBGWRT_ERR((DBH,"*** %s_CfgInfo: code=0x%x not supported\n",
					BBNAME,code));
		va_end( argptr );
		return ERR_BBIS_UNK_CODE;
    }

    va_end( argptr );
    return status;
}

/****************************** A12_IrqEnable ********************************
 *
 *  Description:  Interrupt enable / disable.
 *
 *                For QSPI, nothing is done. QSPI has seperate IRQ9
 *
 *---------------------------------------------------------------------------
 *  Input......:  h    pointer to board handle structure
 *                mSlot     module slot number
 *                enable    interrupt setting
 *  Output.....:  return    0
 *  Globals....:  ---
 ****************************************************************************/
static int32 A12_IrqEnable(
    BBIS_HANDLE     *h,
    u_int32         mSlot,
    u_int32         enable )
{
    DBGWRT_1((DBH, "BB - %s_IrqEnable: mSlot=%d enable=%d\n",
			  BBNAME,mSlot,enable));

	if( mSlot < A12_NBR_OF_MMODS ){
		if( enable )
			MSETMASK_D8( h->mmod[mSlot].vCtrlBase, 0, 0x2 ); /* set IEN */
		else
			MCLRMASK_D8( h->mmod[mSlot].vCtrlBase, 0, 0x2 ); /* clear IEN */
	}

	return 0;
}

/****************************** A12_IrqSrvInit *******************************
 *
 *  Description:  Called at the beginning of an interrupt.
 *
 *                checks if the slot caused the interrupt
 *
 *---------------------------------------------------------------------------
 *  Input......:  h    pointer to board handle structure
 *                mSlot     module slot number
 *  Output.....:  return    BBIS_IRQ_NO
 *  Globals....:  ---
 ****************************************************************************/
static int32 A12_IrqSrvInit(
    BBIS_HANDLE     *h,
    u_int32         mSlot)
{
	IDBGWRT_1((DBH, "BB - %s_IrqSrvInit: mSlot=%d\n",BBNAME,mSlot));

	if( (mSlot==0x1000) ||
		(MREAD_D8( h->mmod[CFIDX(mSlot)].vCtrlBase, 0 ) & 0x1 ))
		return BBIS_IRQ_YES;
	else
		return BBIS_IRQ_NO;
}

/****************************** A12_IrqSrvExit *******************************
 *
 *  Description:  Called at the end of an interrupt.
 *
 *                Do nothing
 *
 *---------------------------------------------------------------------------
 *  Input......:  h    pointer to board handle structure
 *                mSlot     module slot number
 *  Output.....:  ---
 *  Globals....:  ---
 ****************************************************************************/
static void A12_IrqSrvExit(
    BBIS_HANDLE     *h,
    u_int32         mSlot )
{
	IDBGWRT_1((DBH, "BB - %s_IrqSrvExit: mSlot=%d\n",BBNAME,mSlot));
}

/****************************** A12_ExpEnable ********************************
 *
 *  Description:  Exception interrupt enable / disable.
 *
 *                Do nothing
 *
 *---------------------------------------------------------------------------
 *  Input......:  h    pointer to board handle structure
 *                mSlot     module slot number
 *                enable    interrupt setting
 *  Output.....:  return    0
 *  Globals....:  ---
 ****************************************************************************/
static int32 A12_ExpEnable(
    BBIS_HANDLE     *h,
    u_int32         mSlot,
	u_int32			enable)
{
	IDBGWRT_1((DBH, "BB - %s_ExpEnable: mSlot=%d\n",BBNAME,mSlot));

	return 0;
}

/****************************** A12_ExpSrv ***********************************
 *
 *  Description:  Called at the beginning of an exception interrupt.
 *
 *                Do nothing
 *
 *---------------------------------------------------------------------------
 *  Input......:  h    pointer to board handle structure
 *                mSlot     module slot number
 *  Output.....:  return    BBIS_IRQ_NO
 *  Globals....:  ---
 ****************************************************************************/
static int32 A12_ExpSrv(
    BBIS_HANDLE     *h,
    u_int32         mSlot )
{
	IDBGWRT_1((DBH, "BB - %s_ExpSrv: mSlot=%d\n",BBNAME,mSlot));

	return BBIS_IRQ_NO;
}

/****************************** A12_SetMIface ********************************
 *
 *  Description:  Set device interface.
 *
 *                Do nothing
 *
 *---------------------------------------------------------------------------
 *  Input......:  h    pointer to board handle structure
 *                mSlot     module slot number
 *                addrMode  MDIS_MODE_A08 | MDIS_MODE_A24
 *                dataMode  MDIS_MODE_D16 | MDIS_MODE_D32
 *  Output.....:  return    0
 *  Globals....:  ---
 ****************************************************************************/
static int32 A12_SetMIface(
    BBIS_HANDLE     *h,
    u_int32         mSlot,
    u_int32         addrMode,
    u_int32         dataMode)
{
	DBGWRT_1((DBH, "BB - %s_SetMIface: mSlot=%d\n",BBNAME,mSlot));

    return 0;
}

/****************************** A12_ClrMIface ********************************
 *
 *  Description:  Clear device interface.
 *
 *                Do nothing
 *
 *---------------------------------------------------------------------------
 *  Input......:  h    pointer to board handle structure
 *                mSlot     module slot number
 *  Output.....:  return    0
 *  Globals....:  ---
 ****************************************************************************/
static int32 A12_ClrMIface(
    BBIS_HANDLE     *h,
    u_int32         mSlot)
{
	DBGWRT_1((DBH, "BB - %s_ClrMIface: mSlot=%d\n",BBNAME,mSlot));

    return 0;
}

/****************************** A12_GetMAddr *********************************
 *
 *  Description:  Get physical address description.
 *
 *                - check device number
 *                - assign address spaces
 *
 *---------------------------------------------------------------------------
 *  Input......:  h    pointer to board handle structure
 *                mSlot     module slot number
 *                addrMode  MDIS_MA08 | MDIS_MA24
 *                dataMode  MDIS_MD16 | MDIS_MD32
 *                mAddr     pointer to address space
 *                mSize     size of address space
 *  Output.....:  return    0 | error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 A12_GetMAddr(
    BBIS_HANDLE     *h,
    u_int32         mSlot,
    u_int32         addrMode,
    u_int32         dataMode,
    void            **mAddr,
    u_int32         *mSize )
{
	int32 cfgIdx;
	u_int32 size=0;
	INT32_OR_64 base;

	DBGWRT_1((DBH, "BB - %s_GetMAddr: mSlot=%d\n",BBNAME,mSlot));

	cfgIdx = CFIDX(mSlot);
	/* check device number */
	if( (cfgIdx<0) || (cfgIdx >= BRD_NBR_OF_BRDDEV )){
		DBGWRT_ERR((DBH,"*** %s_GetMaddr: mSlot=0x%x not supported\n",
					BBNAME,mSlot));
		return ERR_BBIS_ILL_SLOT;
	}

	base = (INT32_OR_64)h->physBase + (cfgIdx * A12_MMOD_SLOT_OFFSET);

	if( cfgIdx < A12_NBR_OF_MMODS ){
		/* M-module slots */

	    switch (addrMode){
		case MDIS_MA08:
			switch(dataMode){
			case MDIS_MD16:
			case MDIS_MD08:
				base += A12_MMOD_A08_D16_BASE;
				size = 0x100;
				break;
			case MDIS_MD32:
				base += A12_MMOD_A08_D32_BASE;
				size = 0x100;
				break;
			default:
				base = 0xffffffff;
				break;
			}
			break;

		case MDIS_MA24:
			switch(dataMode){
			case MDIS_MD16:
			case MDIS_MD08:
				base += A12_MMOD_A24_D16_BASE;
				size = 0x1000000 - 0x300;
				break;
			case MDIS_MD32:
				base += A12_MMOD_A24_D32_BASE;
				size = 0x1000000;
				break;
			default:
				base = 0xffffffff;
				break;
			}
			break;
		}
	}
	else {
		/* QSPI */
		size = 0x800;
	}

	if( base == 0xffffffff ){
		DBGWRT_ERR((DBH,"*** %s_GetMAddr: addrMode=0x%x/dataMode=0x%x "
					"not supported\n",
					BBNAME,addrMode, dataMode));
        return ERR_BBIS_ILL_ADDRMODE;
	}

	/* assign address spaces */
	*mAddr = (void *)base;
    *mSize = size;

	DBGWRT_2((DBH, " mSlot:0x%x : phys address=0x%x, length=0x%x\n",
		mSlot, *mAddr, *mSize));

    return 0;
}

/****************************** A12_SetStat **********************************
 *
 *  Description:  Set driver status
 *
 *                Following status codes are supported:
 *
 *                Code                 Description                Values
 *                -------------------  -------------------------  ----------
 *                M_BB_DEBUG_LEVEL     board debug level          see dbg.h
 *
 *---------------------------------------------------------------------------
 *  Input......:  h    pointer to board handle structure
 *                mSlot     module slot number
 *                code      setstat code
 *                value     setstat value or ptr to blocksetstat data
 *  Output.....:  return    0 | error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 A12_SetStat(
    BBIS_HANDLE     *h,
    u_int32         mSlot,
    int32           code,
	INT32_OR_64     value )
{
    DBGWRT_1((DBH, "BB - %s_SetStat: mSlot=%d code=0x%04x value=0x%x\n",
			  BBNAME, mSlot, code, value));

    switch (code) {
        /* set debug level */
        case M_BB_DEBUG_LEVEL:
            h->debugLevel = value;
            break;

        /* unknown */
        default:
            return ERR_BBIS_UNK_CODE;
    }

    return 0;
}

/****************************** A12_GetStat **********************************
 *
 *  Description:  Get driver status
 *
 *                Following status codes are supported:
 *
 *                Code                 Description                Values
 *                -------------------  -------------------------  ----------
 *                M_BB_DEBUG_LEVEL     driver debug level         see dbg.h
 *                M_BB_IRQ_VECT        interrupt vector           0..max
 *                M_BB_IRQ_LEVEL       interrupt level            0..max
 *                M_BB_IRQ_PRIORITY    interrupt priority         0
 *                M_MK_BLK_REV_ID      ident function table ptr   -
 *
 *---------------------------------------------------------------------------
 *  Input......:  h    pointer to board handle structure
 *                mSlot     module slot number
 *                code      getstat code
 *  Output.....:  valueP    getstat value or ptr to blockgetstat data
 *                return    0 | error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 A12_GetStat(
    BBIS_HANDLE     *h,
    u_int32         mSlot,
    int32           code,
	INT32_OR_64     *valueP )
{
    int32       status=0;

    DBGWRT_1((DBH, "BB - %s_GetStat: mSlot=%d code=0x%04x\n",BBNAME,mSlot,code));

    switch (code) {
        /* get debug level */
        case M_BB_DEBUG_LEVEL:
            *valueP = h->debugLevel;
            break;

        /* get IRQ vector */
        case M_BB_IRQ_VECT:
		{
			u_int32 vector, level, mode;

			status = A12_CfgInfo( h, BBIS_CFGINFO_IRQ, mSlot,
								  &vector, &level, &mode );

			*valueP = (int32)vector;
			break;
		}
        /* get IRQ level */
        case M_BB_IRQ_LEVEL:
		{
			u_int32 vector, level, mode;

			status = A12_CfgInfo( h, BBIS_CFGINFO_IRQ, mSlot,
								  &vector, &level, &mode );

			*valueP = (int32)level;
			break;
		}

        /* get IRQ priority */
        case M_BB_IRQ_PRIORITY:
            *valueP = 0;
            break;

        /* ident table */
        case M_MK_BLK_REV_ID:
           *valueP = (INT32_OR_64)&h->idFuncTbl;
           break;

        /* unknown */
        default:
            status = ERR_BBIS_UNK_CODE;
    }

    return status;
}

/****************************** A12_Unused ***********************************
 *
 *  Description:  Dummy function for unused jump table entries.
 *
 *---------------------------------------------------------------------------
 *  Input......:  ---
 *  Output.....:  return  ERR_BBIS_ILL_FUNC
 *  Globals....:  ---
 ****************************************************************************/
static int32 A12_Unused( void )		/* nodoc */
{
    return ERR_BBIS_ILL_FUNC;
}

/*********************************** Ident **********************************
 *
 *  Description:  Return ident string
 *
 *---------------------------------------------------------------------------
 *  Input......:  -
 *  Output.....:  return  pointer to ident string
 *  Globals....:  -
 ****************************************************************************/
static char* Ident( void )		/* nodoc */
{
	return( (char*) IdentString );
}

/********************************* Cleanup **********************************
 *
 *  Description:  Close all handles, free memory and return error code
 *
 *		          NOTE: The h handle is invalid after calling this
 *                      function.
 *
 *---------------------------------------------------------------------------
 *  Input......:  h    pointer to board handle structure
 *                retCode	return value
 *  Output.....:  return	retCode
 *  Globals....:  -
 ****************************************************************************/
static int32 Cleanup(
   BBIS_HANDLE  *h,
   int32        retCode		/* nodoc */
)
{
	int32 i;

    /*------------------------------+
    |  close handles                |
    +------------------------------*/
	/* clean up desc */
	if (h->descHdl)
		DESC_Exit(&h->descHdl);

	/* unmap control registers */
	for( i=0; i<A12_NBR_OF_MMODS; i++ ){
		if( h->mmod[i].vCtrlBase )
			OSS_UnMapVirtAddr( h->osHdl, (void **)&h->mmod[i].vCtrlBase,
							   A12_CTRL_SIZE, OSS_ADDRSPACE_MEM );
	}

#ifdef OSS_HAS_UNASSIGN_RESOURCES
	if( h->resourcesAssigned )
		OSS_UnAssignResources( h->osHdl, OSS_BUSTYPE_PCI, 0,
							   A12_NBR_OF_MMODS, h->res );
#endif
	/* cleanup debug */
	DBGEXIT((&DBH));

    /*------------------------------+
    |  free memory                  |
    +------------------------------*/
    /* release memory for the board handle */
    OSS_MemFree( h->osHdl, (int8*)h, h->ownMemSize);
    h = NULL;

    /*------------------------------+
    |  return error code            |
    +------------------------------*/
	return(retCode);
}



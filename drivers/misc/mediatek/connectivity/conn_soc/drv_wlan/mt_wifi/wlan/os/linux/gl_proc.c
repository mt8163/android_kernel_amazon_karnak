/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/linux/gl_proc.c#1
*/

/*! \file   "gl_proc.c"
    \brief  This file defines the interface which can interact with users in /proc fs.

    Detail description.
*/

/*
** Log: gl_proc.c
 *
 * 11 10 2011 cp.wu
 * [WCXRP00001098] [MT6620 Wi-Fi][Driver] Replace printk by DBG LOG macros in linux porting layer
 * 1. eliminaite direct calls to printk in porting layer.
 * 2. replaced by DBGLOG, which would be XLOG on ALPS platforms.
 *
 * 12 10 2010 kevin.huang
 * [WCXRP00000128] [MT6620 Wi-Fi][Driver] Add proc support to Android Driver for debug and driver status check
 * Add Linux Proc Support
**  \main\maintrunk.MT5921\19 2008-09-02 21:08:37 GMT mtk01461
**  Fix the compile error of SPRINTF()
**  \main\maintrunk.MT5921\18 2008-08-10 18:48:28 GMT mtk01461
**  Update for Driver Review
**  \main\maintrunk.MT5921\17 2008-08-04 16:52:01 GMT mtk01461
**  Add proc dbg print message of DOMAIN_INDEX level
**  \main\maintrunk.MT5921\16 2008-07-10 00:45:16 GMT mtk01461
**  Remove the check of MCR offset, we may use the MCR address which is not align to DW boundary or proprietary usage.
**  \main\maintrunk.MT5921\15 2008-06-03 20:49:44 GMT mtk01461
**  \main\maintrunk.MT5921\14 2008-06-02 22:56:00 GMT mtk01461
**  Rename some functions for linux proc
**  \main\maintrunk.MT5921\13 2008-06-02 20:23:18 GMT mtk01461
**  Revise PROC mcr read / write for supporting TELNET
**  \main\maintrunk.MT5921\12 2008-03-28 10:40:25 GMT mtk01461
**  Remove temporary set desired rate in linux proc
**  \main\maintrunk.MT5921\11 2008-01-07 15:07:29 GMT mtk01461
**  Add User Update Desired Rate Set for QA in Linux
**  \main\maintrunk.MT5921\10 2007-12-11 00:11:14 GMT mtk01461
**  Fix SPIN_LOCK protection
**  \main\maintrunk.MT5921\9 2007-12-04 18:07:57 GMT mtk01461
**  Add additional debug category to proc
**  \main\maintrunk.MT5921\8 2007-11-02 01:03:23 GMT mtk01461
**  Unify TX Path for Normal and IBSS Power Save + IBSS neighbor learning
**  \main\maintrunk.MT5921\7 2007-10-25 18:08:14 GMT mtk01461
**  Add VOIP SCAN Support  & Refine Roaming
** Revision 1.3  2007/07/05 07:25:33  MTK01461
** Add Linux initial code, modify doc, add 11BB, RF init code
**
** Revision 1.2  2007/06/27 02:18:51  MTK01461
** Update SCAN_FSM, Initial(Can Load Module), Proc(Can do Reg R/W), TX API
**
*/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#include "precomp.h"

/* #include "wlan_lib.h" */
/* #include "debug.h" */

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

#define PROC_WLAN_CHIP_ID						"chipid"
#define PROC_VERSION							"FWVersion"
#define PROC_WLAN_MCR							"MCR"
#define PROC_WLAN_THERMO                        "wlanThermo"
#define PROC_COUNTRY                            "country"
#define PROC_ROAM_PARAM                         "roam_param"
#define PROC_DRV_STATUS                         "status"
#define PROC_RX_BA_WIN                          "RxBaWin"
#define PROC_RX_STATISTICS                      "rx_statistics"
#define PROC_TX_STATISTICS                      "tx_statistics"
#define PROC_DBG_LEVEL_NAME              "dbgLevel"
#define PROC_ANTENNA_SELECT             "antenna_select"
#define PROC_ANTENNA_RSSI		"antenna_query"
#define PROC_DTIM "dtim_skip_count"
#define PROC_ROOT_NAME			"wlan"

#define PROC_ANTENNA_SELECT_MAX_USER_INPUT_LEN  20
#define PROC_MCR_ACCESS_MAX_USER_INPUT_LEN      20
#define PROC_RX_STATISTICS_MAX_USER_INPUT_LEN   10
#define PROC_TX_STATISTICS_MAX_USER_INPUT_LEN   10
#define PROC_DBG_LEVEL_MAX_USER_INPUT_LEN       (20*10)
#define PROC_DBG_LEVEL_MAX_DISPLAY_STR_LEN      8

#define PROC_UID_SHELL							2000
#define PROC_GID_WIFI							1010

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
static UINT_32 u4McrOffset = 0x0;
static UINT_16 u2RxBaWin = 0x0;
#if CFG_SUPPORT_THERMO_THROTTLING
static P_GLUE_INFO_T g_prGlueInfo_proc;
#endif

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

static ssize_t
procRxBAWinRead(
struct file *filp,
char __user *buf,
size_t count,
loff_t *f_pos)
{
	UINT_32 u4CopySize = 0;
	UINT_16 u2RxBa;
	UINT_8 aucProcBuf[42];

	if (*f_pos > 0)
		return 0;	/* To indicate end of file. */;

	u2RxBa = u2RxBaWin;
	if (!u2RxBaWin)
		kalSprintf(aucProcBuf, "RxBaWin : 0x%x(disable)\n", u2RxBa);
	else
		kalSprintf(aucProcBuf, "RxBaWin : 0x%x\n", u2RxBa);
	u4CopySize = kalStrLen(aucProcBuf);
	if (u4CopySize > count)
		u4CopySize = count;

	if (copy_to_user(buf, aucProcBuf, u4CopySize)) {
		pr_err("copy to user failed\n");
		return -EFAULT;
	}
	*f_pos += u4CopySize;

	return (ssize_t)u4CopySize;
}

static ssize_t
procRxBAWinWrite(
struct file *file,
const char __user *buffer,
size_t count,
loff_t *data)
{
	UINT8 acBuf[21];
	int i4CopySize;
	PARAM_CUSTOM_SW_CTRL_STRUCT_T uSwCtrlInfo;
	UINT_32 u4BufLen;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT8 *temp = &acBuf[0];

	if (count < (sizeof(acBuf) - 1))
		i4CopySize = count;
	else
		i4CopySize = (sizeof(acBuf) - 1);

	if (copy_from_user(acBuf, buffer, i4CopySize)) {
		pr_err("error of copy from user\n");
		return -EFAULT;
	}
	acBuf[i4CopySize] = '\0';

	rStatus = sscanf(temp, "0x%hx", &u2RxBaWin);

	if (u2RxBaWin & 0x08)
		u2RxBaWin &= 0x18;
	else if (u2RxBaWin & 0x04)
		u2RxBaWin &= 0x14;
	else if (u2RxBaWin & 0x02)
		u2RxBaWin &= 0x12;
	else if (u2RxBaWin & 0x01)
		u2RxBaWin &= 0x11;

	uSwCtrlInfo.u4Data = (UINT_32)u2RxBaWin;
	uSwCtrlInfo.u4Id = 0x30000000;

	if (rStatus == 1) {
		rStatus = kalIoctl(g_prGlueInfo_proc,
			(PFN_OID_HANDLER_FUNC) wlanoidSetSwCtrlWrite,
			(PVOID)&uSwCtrlInfo,
			sizeof(uSwCtrlInfo),
			FALSE, FALSE, TRUE, FALSE,
			&u4BufLen);
		if (rStatus == WLAN_STATUS_FAILURE) {
			DBGLOG(INIT, INFO, "write RxBaWin failed 0x%x\n", rStatus);
			return -EINVAL;
		}
	}
	return (ssize_t)count;
}

static const struct file_operations rxba_fops = {
	.owner = THIS_MODULE,
	.write = procRxBAWinWrite,
	.read = procRxBAWinRead,
};

/*----------------------------------------------------------------------------*/
/*!
* \brief The PROC function for reading MCR register to User Space, the offset of
*        the MCR is specified in u4McrOffset.
*
* \param[in] page       Buffer provided by kernel.
* \param[in out] start  Start Address to read(3 methods).
* \param[in] off        Offset.
* \param[in] count      Allowable number to read.
* \param[out] eof       End of File indication.
* \param[in] data       Pointer to the private data structure.
*
* \return number of characters print to the buffer from User Space.
*/
/*----------------------------------------------------------------------------*/

#if 1
static ssize_t procMCRRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	UINT_32 u4BufLen = 0;
	UINT_32 u4CopySize = 0;
	PARAM_CUSTOM_MCR_RW_STRUCT_T rMcrInfo;
	WLAN_STATUS rStatus;
	UINT_8 aucProcBuf[42];

	if (*f_pos > 0)
		return 0;	/* To indicate end of file. */;

	rMcrInfo.u4McrOffset = u4McrOffset;

	rStatus = kalIoctl(g_prGlueInfo_proc,
			   wlanoidQueryMcrRead,
			   (PVOID)&rMcrInfo, sizeof(rMcrInfo), TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO, "read mcr failed\n");
		return -EINVAL;
	}

	kalSprintf(aucProcBuf, "Offset : 0x%08x, Value : 0x%08x\n",
		rMcrInfo.u4McrOffset, rMcrInfo.u4McrData);
	u4CopySize = kalStrLen(aucProcBuf);
	if (u4CopySize > count)
		u4CopySize = count;

	if (copy_to_user(buf, aucProcBuf, u4CopySize)) {
		pr_err("copy to user failed\n");
		return -EFAULT;
	}
	*f_pos += u4CopySize;

	return (ssize_t)u4CopySize;
}				/* end of procMCRRead() */

/*----------------------------------------------------------------------------*/
/*!
* \brief The PROC function for writing MCR register to HW or update u4McrOffset
*        for reading MCR later.
*
* \param[in] file   pointer to file.
* \param[in] buffer Buffer from user space.
* \param[in] count  Number of characters to write
* \param[in] data   Pointer to the private data structure.
*
* \return number of characters write from User Space.
*/
/*----------------------------------------------------------------------------*/
static ssize_t procMCRWrite(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	UINT8 acBuf[PROC_MCR_ACCESS_MAX_USER_INPUT_LEN + 1];	/* + 1 for "\0" */
	int i4CopySize;
	PARAM_CUSTOM_MCR_RW_STRUCT_T rMcrInfo;
	UINT_32 u4BufLen, u4McrData = 0;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT8 *temp = &acBuf[0];

	i4CopySize = (count < (sizeof(acBuf) - 1)) ? count : (sizeof(acBuf) - 1);

	if (copy_from_user(acBuf, buffer, i4CopySize)) {
		pr_err("error of copy from user\n");
		return -EFAULT;
	}
	acBuf[i4CopySize] = '\0';

	rStatus = sscanf(temp, "0x%x 0x%x", &u4McrOffset, &u4McrData);
	rMcrInfo.u4McrOffset = u4McrOffset;
	rMcrInfo.u4McrData = u4McrData;

	if (rStatus == 2) {
		/* NOTE: Sometimes we want to test if bus will still be ok, after accessing
		 * the MCR which is not align to DW boundary.
		 */
		/* if (IS_ALIGN_4(rMcrInfo.u4McrOffset)) */
		rStatus = kalIoctl(g_prGlueInfo_proc,
				wlanoidSetMcrWrite,
				(PVOID)&rMcrInfo, sizeof(rMcrInfo), FALSE, FALSE, TRUE, FALSE, &u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, INFO, "write mcr failed\n");
			return -EINVAL;
		}
	}
	return (ssize_t)count;
}				/* end of procMCRWrite() */

static const struct file_operations mcr_fops = {
	.owner = THIS_MODULE,
	.write = procMCRWrite,
	.read = procMCRRead,
};

#endif

#if 0
/*----------------------------------------------------------------------------*/
/*!
* \brief The PROC function for reading Driver Status to User Space.
*
* \param[in] page       Buffer provided by kernel.
* \param[in out] start  Start Address to read(3 methods).
* \param[in] off        Offset.
* \param[in] count      Allowable number to read.
* \param[out] eof       End of File indication.
* \param[in] data       Pointer to the private data structure.
*
* \return number of characters print to the buffer from User Space.
*/
/*----------------------------------------------------------------------------*/
static int procDrvStatusRead(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	P_GLUE_INFO_T prGlueInfo = ((struct net_device *)data)->priv;
	char *p = page;
	UINT_32 u4Count;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(data);

	/* Kevin: Apply PROC read method 1. */
	if (off != 0)
		return 0;	/* To indicate end of file. */

	SPRINTF(p, ("GLUE LAYER STATUS:"));
	SPRINTF(p, ("\n=================="));

	SPRINTF(p, ("\n* Number of Pending Frames: %ld\n", prGlueInfo->u4TxPendingFrameNum));

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

	wlanoidQueryDrvStatusForLinuxProc(prGlueInfo->prAdapter, p, &u4Count);

	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

	u4Count += (UINT_32) (p - page);

	*eof = 1;

	return (int)u4Count;

}				/* end of procDrvStatusRead() */

/*----------------------------------------------------------------------------*/
/*!
* \brief The PROC function for reading Driver RX Statistic Counters to User Space.
*
* \param[in] page       Buffer provided by kernel.
* \param[in out] start  Start Address to read(3 methods).
* \param[in] off        Offset.
* \param[in] count      Allowable number to read.
* \param[out] eof       End of File indication.
* \param[in] data       Pointer to the private data structure.
*
* \return number of characters print to the buffer from User Space.
*/
/*----------------------------------------------------------------------------*/
static int procRxStatisticsRead(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	P_GLUE_INFO_T prGlueInfo = ((struct net_device *)data)->priv;
	char *p = page;
	UINT_32 u4Count;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(data);

	/* Kevin: Apply PROC read method 1. */
	if (off != 0)
		return 0;	/* To indicate end of file. */

	SPRINTF(p, ("RX STATISTICS (Write 1 to clear):"));
	SPRINTF(p, ("\n=================================\n"));

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

	wlanoidQueryRxStatisticsForLinuxProc(prGlueInfo->prAdapter, p, &u4Count);

	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

	u4Count += (UINT_32) (p - page);

	*eof = 1;

	return (int)u4Count;

}				/* end of procRxStatisticsRead() */

/*----------------------------------------------------------------------------*/
/*!
* \brief The PROC function for reset Driver RX Statistic Counters.
*
* \param[in] file   pointer to file.
* \param[in] buffer Buffer from user space.
* \param[in] count  Number of characters to write
* \param[in] data   Pointer to the private data structure.
*
* \return number of characters write from User Space.
*/
/*----------------------------------------------------------------------------*/
static int procRxStatisticsWrite(struct file *file, const char *buffer, unsigned long count, void *data)
{
	P_GLUE_INFO_T prGlueInfo = ((struct net_device *)data)->priv;
	char acBuf[PROC_RX_STATISTICS_MAX_USER_INPUT_LEN + 1];	/* + 1 for "\0" */
	UINT_32 u4CopySize;
	UINT_32 u4ClearCounter;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(data);

	u4CopySize = (count < (sizeof(acBuf) - 1)) ? count : (sizeof(acBuf) - 1);
	copy_from_user(acBuf, buffer, u4CopySize);
	acBuf[u4CopySize] = '\0';

	if (kstrtoint(acBuf, 10, &u4ClearCounter) == 1) {
		if (u4ClearCounter == 1) {
			GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

			wlanoidSetRxStatisticsForLinuxProc(prGlueInfo->prAdapter);

			GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);
		}
	}

	return count;

}				/* end of procRxStatisticsWrite() */

/*----------------------------------------------------------------------------*/
/*!
* \brief The PROC function for reading Driver TX Statistic Counters to User Space.
*
* \param[in] page       Buffer provided by kernel.
* \param[in out] start  Start Address to read(3 methods).
* \param[in] off        Offset.
* \param[in] count      Allowable number to read.
* \param[out] eof       End of File indication.
* \param[in] data       Pointer to the private data structure.
*
* \return number of characters print to the buffer from User Space.
*/
/*----------------------------------------------------------------------------*/
static int procTxStatisticsRead(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	P_GLUE_INFO_T prGlueInfo = ((struct net_device *)data)->priv;
	char *p = page;
	UINT_32 u4Count;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(data);

	/* Kevin: Apply PROC read method 1. */
	if (off != 0)
		return 0;	/* To indicate end of file. */

	SPRINTF(p, ("TX STATISTICS (Write 1 to clear):"));
	SPRINTF(p, ("\n=================================\n"));

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

	wlanoidQueryTxStatisticsForLinuxProc(prGlueInfo->prAdapter, p, &u4Count);

	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

	u4Count += (UINT_32) (p - page);

	*eof = 1;

	return (int)u4Count;

}				/* end of procTxStatisticsRead() */

/*----------------------------------------------------------------------------*/
/*!
* \brief The PROC function for reset Driver TX Statistic Counters.
*
* \param[in] file   pointer to file.
* \param[in] buffer Buffer from user space.
* \param[in] count  Number of characters to write
* \param[in] data   Pointer to the private data structure.
*
* \return number of characters write from User Space.
*/
/*----------------------------------------------------------------------------*/
static int procTxStatisticsWrite(struct file *file, const char *buffer, unsigned long count, void *data)
{
	P_GLUE_INFO_T prGlueInfo = ((struct net_device *)data)->priv;
	char acBuf[PROC_RX_STATISTICS_MAX_USER_INPUT_LEN + 1];	/* + 1 for "\0" */
	UINT_32 u4CopySize;
	UINT_32 u4ClearCounter;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(data);

	u4CopySize = (count < (sizeof(acBuf) - 1)) ? count : (sizeof(acBuf) - 1);
	copy_from_user(acBuf, buffer, u4CopySize);
	acBuf[u4CopySize] = '\0';

	if (kstrtoint(acBuf, 10, &u4ClearCounter) == 1) {
		if (u4ClearCounter == 1) {
			GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

			wlanoidSetTxStatisticsForLinuxProc(prGlueInfo->prAdapter);

			GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);
		}
	}

	return count;

}				/* end of procTxStatisticsWrite() */
#endif
static struct proc_dir_entry *gprProcRoot;
static UINT_8 aucDbModuleName[][PROC_DBG_LEVEL_MAX_DISPLAY_STR_LEN] = {
	"INIT", "HAL", "INTR", "REQ", "TX", "RX", "RFTEST", "EMU", "SW1", "SW2",
	"SW3", "SW4", "HEM", "AIS", "RLM", "MEM", "CNM", "RSN", "BSS", "SCN",
	"SAA", "AAA", "P2P", "QM", "SEC", "BOW", "WAPI", "ROAMING", "TDLS", "OID",
	"NIC", "ANT", "SSID"
};

static UINT_8 aucProcBuf[1536];
static ssize_t procDbgLevelRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	UINT_8 *temp = &aucProcBuf[0];
	UINT_32 u4CopySize = 0;
	UINT_16 i;
	UINT_16 u2ModuleNum = 0;

	/* if *f_ops>0, we should return 0 to make cat command exit */
	if (*f_pos > 0)
		return 0;

	kalStrCpy(temp, "\nERROR|WARN|STATE|EVENT|TRACE|INFO|LOUD|TEMP\n"
			"bit0 |bit1|bit2 |bit3 |bit4 |bit5|bit6|bit7\n\n"
			"Debug Module\tIndex\tLevel\tDebug Module\tIndex\tLevel\n\n");
	temp += kalStrLen(temp);

	u2ModuleNum = (sizeof(aucDbModuleName) / PROC_DBG_LEVEL_MAX_DISPLAY_STR_LEN) & 0xfe;
	for (i = 0; i < u2ModuleNum; i += 2)
		SPRINTF(temp, ("DBG_%s_IDX\t(0x%02x):\t0x%02x\tDBG_%s_IDX\t(0x%02x):\t0x%02x\n",
				&aucDbModuleName[i][0], i, aucDebugModule[i],
				&aucDbModuleName[i+1][0], i+1, aucDebugModule[i+1]));

	if ((sizeof(aucDbModuleName) / PROC_DBG_LEVEL_MAX_DISPLAY_STR_LEN) & 0x1)
		SPRINTF(temp, ("DBG_%s_IDX\t(0x%02x):\t0x%02x\n",
				&aucDbModuleName[u2ModuleNum][0], u2ModuleNum, aucDebugModule[u2ModuleNum]));

	u4CopySize = kalStrLen(aucProcBuf);
	if (u4CopySize > count)
		u4CopySize = count;
	if (copy_to_user(buf, aucProcBuf, u4CopySize)) {
		pr_err("copy to user failed\n");
		return -EFAULT;
	}

	*f_pos += u4CopySize;
	return (ssize_t)u4CopySize;
}

static ssize_t procDbgLevelWrite(struct file *file, const char *buffer, size_t count, loff_t *data)
{
	UINT_32 u4NewDbgModule, u4NewDbgLevel;
	UINT_8 i = 0;
	UINT_32 u4CopySize = sizeof(aucProcBuf);
	UINT_8 *temp = &aucProcBuf[0];

	kalMemSet(aucProcBuf, 0, u4CopySize);
	u4CopySize = (count < u4CopySize) ? count : (u4CopySize - 1);

	if (copy_from_user(aucProcBuf, buffer, u4CopySize)) {
		pr_err("error of copy from user\n");
		return -EFAULT;
	}
	aucProcBuf[u4CopySize] = '\0';

	while (temp) {
		if (sscanf(temp, "0x%x:0x%x", &u4NewDbgModule, &u4NewDbgLevel) != 2)  {
			pr_info("debug module and debug level should be one byte in length\n");
			break;
		}
		if (u4NewDbgModule == 0xFF) {
			for (i = 0; i < DBG_MODULE_NUM; i++)
				aucDebugModule[i] = u4NewDbgLevel & DBG_CLASS_MASK;

			break;
		} else if (u4NewDbgModule >= DBG_MODULE_NUM) {
			pr_info("debug module index should less than %d\n", DBG_MODULE_NUM);
			break;
		}
		aucDebugModule[u4NewDbgModule] =  u4NewDbgLevel & DBG_CLASS_MASK;
		temp = kalStrChr(temp, ',');
		if (!temp)
			break;
		temp++; /* skip ',' */
	}
	return count;
}


static const struct file_operations dbglevel_ops = {
	.owner = THIS_MODULE,
	.read = procDbgLevelRead,
	.write = procDbgLevelWrite,
};

/*----------------------------------------------------------------------------*/
/*!
* \brief This function create a PROC fs in linux /proc/net subdirectory.
*
* \param[in] prDev      Pointer to the struct net_device.
* \param[in] pucDevName Pointer to the name of net_device.
*
* \return N/A
*/
/*----------------------------------------------------------------------------*/

#if CFG_SUPPORT_THERMO_THROTTLING

/**
 * This function is called then the /proc file is read
 *
 */
typedef struct _COEX_BUF1 {
	UINT8 buffer[128];
	INT32 availSize;
} COEX_BUF1, *P_COEX_BUF1;

COEX_BUF1 gCoexBuf1;

static ssize_t procfile_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{

	INT32 retval = 0;
	INT32 i_ret = 0;
	CHAR *warn_msg = "no data available, please run echo 15 xx > /proc/driver/wmt_psm first\n";

	if (*f_pos > 0) {
		retval = 0;
	} else {
		/*len = sprintf(page, "%d\n", g_psm_enable); */
		if (gCoexBuf1.availSize <= 0) {
			DBGLOG(INIT, WARN, "no data available\n");
			retval = strlen(warn_msg) + 1;
			if (count < retval)
				retval = count;
			i_ret = copy_to_user(buf, warn_msg, retval);
			if (i_ret) {
				DBGLOG(INIT, ERROR, "copy to buffer failed, ret:%d\n", retval);
				retval = -EFAULT;
				goto err_exit;
			}
			*f_pos += retval;
		} else {
			INT32 i = 0;
			INT32 len = 0;
			CHAR msg_info[128];
			INT32 max_num = 0;
			/*we do not check page buffer, because there are only 100 bytes in g_coex_buf, no reason page
			buffer is not enough, a bomb is placed here on unexpected condition */

			DBGLOG(INIT, TRACE, "%d bytes available\n", gCoexBuf1.availSize);
			max_num = ((sizeof(msg_info) > count ? sizeof(msg_info) : count) - 1) / 5;

			if (max_num > gCoexBuf1.availSize)
				max_num = gCoexBuf1.availSize;
			else
				DBGLOG(INIT, TRACE,
				"round to %d bytes due to local buffer size limitation\n", max_num);

			for (i = 0; i < max_num; i++)
				len += sprintf(msg_info + len, "%d", gCoexBuf1.buffer[i]);

			len += sprintf(msg_info + len, "\n");
			retval = len;

			i_ret = copy_to_user(buf, msg_info, retval);
			if (i_ret) {
				DBGLOG(INIT, ERROR, "copy to buffer failed, ret:%d\n", retval);
				retval = -EFAULT;
				goto err_exit;
			}
			*f_pos += retval;
		}
	}
	gCoexBuf1.availSize = 0;
err_exit:

	return retval;
}

typedef INT32 (*WLAN_DEV_DBG_FUNC)(void);
static INT32 wlan_get_thermo_power(void);
static INT32 wlan_get_link_mode(void);

static const WLAN_DEV_DBG_FUNC wlan_dev_dbg_func[] = {
	[0] = wlan_get_thermo_power,
	[1] = wlan_get_link_mode,

};

INT32 wlan_get_thermo_power(void)
{
	P_ADAPTER_T prAdapter;

	prAdapter = g_prGlueInfo_proc->prAdapter;

	if (prAdapter->u4AirDelayTotal > 100)
		gCoexBuf1.buffer[0] = 100;
	else
		gCoexBuf1.buffer[0] = prAdapter->u4AirDelayTotal;
	gCoexBuf1.availSize = 1;
	DBGLOG(RLM, TRACE, "PROC %s thrmo_power(%d)\n", __func__, gCoexBuf1.buffer[0]);

	return 0;
}

INT32 wlan_get_link_mode(void)
{
	UINT_8 ucLinkMode = 0;
	P_ADAPTER_T prAdapter;
	BOOLEAN fgIsAPmode;

	prAdapter = g_prGlueInfo_proc->prAdapter;
	fgIsAPmode = p2pFuncIsAPMode(prAdapter->rWifiVar.prP2pFsmInfo);

	DBGLOG(RLM, TRACE, "PROC %s AIS(%d)P2P(%d)AP(%d)\n",
			   __func__,
			   prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX].eConnectionState,
			   prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX].eConnectionState, fgIsAPmode);


	if (prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX].eConnectionState == PARAM_MEDIA_STATE_CONNECTED)
		ucLinkMode |= BIT(0);
	if (prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX].eConnectionState == PARAM_MEDIA_STATE_CONNECTED)
		ucLinkMode |= BIT(1);
	if (fgIsAPmode)
		ucLinkMode |= BIT(2);

	gCoexBuf1.buffer[0] = ucLinkMode;
	gCoexBuf1.availSize = 1;

	return 0;
}

static ssize_t procfile_write(struct file *filp, const char __user *buffer,
			size_t count, loff_t *f_pos)
{
	char buf[256];
	char *pBuf;
	ULONG len = count;
	unsigned int x = 0;
	char *pToken = NULL;
	char *pDelimiter = " \t";
	INT32 i4Ret = -1;

	if (count > sizeof(gCoexBuf1.buffer)) {
		DBGLOG(INIT, TRACE, "write data overflow\n");
		return -EINVAL;
	}

	/* return gCoexBuf1.availSize; */
	DBGLOG(INIT, TRACE, "write parameter len = %d\n\r", (INT32) len);
	if (len >= sizeof(buf)) {
		DBGLOG(INIT, ERROR, "input handling fail!\n");
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	DBGLOG(INIT, TRACE, "write parameter data = %s\n\r", buf);

	pBuf = buf;
	pToken = strsep(&pBuf, pDelimiter);

	if (pToken) {
		i4Ret = kalkStrtou32(pToken, 16, &x);
		if (!i4Ret)
			DBGLOG(INIT, TRACE, " x(0x%08x)\n\r", x);
	}

	if ((!i4Ret) &&
	    ((sizeof(wlan_dev_dbg_func) / sizeof(wlan_dev_dbg_func[0])) > x) &&
	    (wlan_dev_dbg_func[x] != NULL))
		(*wlan_dev_dbg_func[x]) ();
	else
		DBGLOG(INIT, ERROR,
		       "no handler defined for command id(0x%08x), pToken=%p, i4Ret=%d\n\r",
		       x, pToken, i4Ret);

	return len;
}
static const struct file_operations proc_fops = {
	.owner = THIS_MODULE,
	.read = procfile_read,
	.write = procfile_write,
};
#endif
static ssize_t procCountryRead(struct file *file, char __user *buf,
				size_t count, loff_t *f_pos)

{
	UINT_32 u4BufLen = 0;
	UINT_32 u4CopySize = 0;
	UINT_16 u2CountryCode = 0;
	WLAN_STATUS rStatus;
	UINT_8 *temp = aucProcBuf;

	if (*f_pos > 0)
		return 0;

	rStatus = kalIoctl(g_prGlueInfo_proc, wlanoidGetCountryCode,
			   &u2CountryCode, sizeof(u2CountryCode), TRUE, FALSE, FALSE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO, "failed get country code");
		return -EINVAL;
	}

	/* country code NULL */
	if (u2CountryCode == 0x0)
		kalStrCpy(aucProcBuf, "Country: NULL\n");
	else
		SPRINTF(temp, ("Country: %c%c\n", (char)((u2CountryCode & 0xFF00) >> 8),
						  (char)(u2CountryCode & 0x00FF)));

	u4CopySize = kalStrLen(aucProcBuf);
	if (u4CopySize > count)
		u4CopySize = count;

	if (copy_to_user(buf, aucProcBuf, u4CopySize)) {
		pr_err("copy to user failed\n");
		return -EFAULT;
	}

	*f_pos += u4CopySize;
	return (ssize_t)u4CopySize;
}

static ssize_t procCountryWrite(struct file *file, const char __user *buffer,
				size_t count, loff_t *data)
{
	UINT_32 u4BufLen = 0;
	WLAN_STATUS rStatus;
	UINT_8 aucCountry[2];
	UINT_32 u4CopySize = sizeof(aucProcBuf);

	aucCountry[0] = aucProcBuf[0];
	aucCountry[1] = aucProcBuf[1];
	if ('X' == aucProcBuf[0] && 'X' == aucProcBuf[1])
		aucProcBuf[0] = aucProcBuf[1] = 'W';

	kalMemSet(aucProcBuf, 0, u4CopySize);
	if (u4CopySize > count)
		u4CopySize = count;
	else
		u4CopySize = u4CopySize - 1;

	if (copy_from_user(aucProcBuf, buffer, u4CopySize)) {
		pr_err("error of copy from user\n");
		return -EFAULT;
	}

	aucProcBuf[u4CopySize] = '\0';
	rStatus = kalIoctl(g_prGlueInfo_proc, wlanoidSetCountryCode,
				&aucProcBuf[0], 2, FALSE, FALSE, TRUE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO, "failed set country code: %s\n", aucProcBuf);
		return -EINVAL;
	}

	rStatus = kalIoctl(g_prGlueInfo_proc, wlanoidUpdatePowerTable,
				&aucProcBuf[0], 2, FALSE, FALSE, TRUE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO, "failed update power table: %c%c\n", aucProcBuf[0], aucProcBuf[1]);
		return -EINVAL;
	}
	/*Indicate channel change notificaiton to wpa_supplicant via cfg80211*/
	if ('W' == aucCountry[0] && 'W' == aucCountry[1])
		aucCountry[0] = aucCountry[1] = 'X';
	wlanRegulatoryHint(&aucCountry[0]);

	return count;
}

static const struct file_operations country_ops = {
	.owner = THIS_MODULE,
	.write = procCountryWrite,
	.read = procCountryRead
};

static ssize_t procRoamRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	UINT_32 u4CopySize;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen = 0;
	/* if *f_pos > 0, it means has read successed last time, don't try again */
	if (*f_pos > 0)
		return 0;
	rStatus = kalIoctl(g_prGlueInfo_proc, wlanoidGetRoamParams, aucProcBuf, sizeof(aucProcBuf),
			   TRUE, FALSE, FALSE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO, "failed to read roam params\n");
		return -EINVAL;
	}

	u4CopySize = kalStrLen(aucProcBuf);
	if (u4CopySize > count)
		u4CopySize = count;

	if (copy_to_user(buf, aucProcBuf, u4CopySize)) {
		pr_err("copy to user failed\n");
		return -EFAULT;
	}

	*f_pos += u4CopySize;

	return (INT_32)u4CopySize;
}

static ssize_t procRoamWrite(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen = 0;
	UINT_32 u4CopySize = sizeof(aucProcBuf);

	kalMemSet(aucProcBuf, 0, u4CopySize);
	if (u4CopySize > count)
		u4CopySize = count;
	else
		u4CopySize = u4CopySize - 1;

	if (copy_from_user(aucProcBuf, buffer, u4CopySize)) {
		pr_err("error of copy from user\n");
		return -EFAULT;
	}

	aucProcBuf[u4CopySize] = '\0';
	if (kalStrnCmp(aucProcBuf, "force_roam", 10) == 0)
		rStatus = kalIoctl(g_prGlueInfo_proc, wlanoidSetForceRoam, NULL, 0,
				   FALSE, FALSE, FALSE, FALSE, &u4BufLen);
	else
		rStatus = kalIoctl(g_prGlueInfo_proc, wlanoidSetRoamParams, aucProcBuf,
				   kalStrLen(aucProcBuf), FALSE, FALSE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO, "failed to set roam params: %s\n", aucProcBuf);
		return -EINVAL;
	}
	return count;
}

static const struct file_operations roam_ops = {
	.owner = THIS_MODULE,
	.read = procRoamRead,
	.write = procRoamWrite,
};

#ifdef CONFIG_MTK_WIFI_ANTENNA_SWITCH
/* Antenna switch to Antenna1, Antenna2, or Auto switch
   mode : ANT1
	  ANT2
	  AUTO
*/
WLAN_STATUS antennaSwitch(P_ADAPTER_T prAdapter, UINT32 mode, bool is_oid)
{
	CMD_SW_DBG_CTRL_T rCmdSwCtrl;
	UINT32 debugId = 0xa0340000 | mode;

	rCmdSwCtrl.u4Id = debugId;
	rCmdSwCtrl.u4Data = 0x0;
	DBGLOG(INIT, TRACE, "antennaSwitch 0x%x, %d\n",
		rCmdSwCtrl.u4Id, rCmdSwCtrl.u4Data);
	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_SW_DBG_CTRL,
				   TRUE,
				   FALSE,
				   is_oid,
				   nicCmdEventSetCommon,
				   nicOidCmdTimeoutCommon,
				   sizeof(CMD_SW_DBG_CTRL_T),
				   (PUINT_8) (&rCmdSwCtrl),
				   NULL,
				   0);
}

static ssize_t antenna_select_write(struct file *filp, const char __user *buf,
					size_t count, loff_t *f_pos)
{
	char buffer[PROC_ANTENNA_SELECT_MAX_USER_INPUT_LEN + 1];
	P_ADAPTER_T prAdapter;

	if(g_prGlueInfo_proc != NULL)
		prAdapter = g_prGlueInfo_proc->prAdapter;
	else
		return -EFAULT;

	if(count > PROC_ANTENNA_SELECT_MAX_USER_INPUT_LEN){
		DBGLOG(INIT, TRACE, "write data overflow\n");
		return -EFAULT;
	}

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	buffer[count] = '\0';
	DBGLOG(INIT, TRACE, "write parameter data = %s\n\r", buffer);
	if(strncmp(buffer, "auto", 4) == 0)
		antennaSwitch(prAdapter, ANT_AUTO, FALSE);
	else if(strncmp(buffer, "0", 1) == 0)
		antennaSwitch(prAdapter, ANT1, FALSE);
	else if(strncmp(buffer, "1", 1) == 0)
		antennaSwitch(prAdapter, ANT2, FALSE);
	else
		DBGLOG(INIT, TRACE, "not a valid input\n\r");

	return count;
}

static const struct file_operations antenna_select_fops = {
	.owner = THIS_MODULE,
	.write = antenna_select_write,
};
#endif

#ifdef CONFIG_MTK_WIFI_ANTENNA_SELECT
static ssize_t antenna_query_read(struct file *filp,
				  char __user *buf,
				  size_t count, loff_t *f_pos)
{
	UINT_32 u4CopySize;
	P_ADAPTER_T prAdapter;
	PARAM_RSSI avg_rssi;
	UINT_32 u4BufLen = 0;
	WLAN_STATUS rStatus;
	struct antenna_select_info *ant_sel;

	if (g_prGlueInfo_proc != NULL)
		prAdapter = g_prGlueInfo_proc->prAdapter;
	else
		return -EFAULT;
	/* if *f_pos > 0, it means has read successed last time,
	 *  don't try again
	 */
	if (*f_pos > 0)
		return 0;
	ant_sel = &(prAdapter->rWifiVar.ant_select_info);

	rStatus = kalIoctl(g_prGlueInfo_proc, wlanoid_get_antenna_rssi,
			   &avg_rssi, sizeof(avg_rssi),
			   TRUE, TRUE, FALSE, FALSE, &u4BufLen);

	kalSprintf(aucProcBuf, "%d\n", avg_rssi);
	DBGLOG(INIT, INFO, "Avg_rssi=%d\n", avg_rssi);

	u4CopySize = kalStrLen(aucProcBuf);
	if (u4CopySize > count)
		u4CopySize = count;

	if (copy_to_user(buf, aucProcBuf, u4CopySize)) {
		pr_err("copy to user failed\n");
		return -EFAULT;
	}

	*f_pos += u4CopySize;

	return (INT_32)u4CopySize;
}

static ssize_t antenna_query_write(struct file *file,
				   const char __user *buffer,
				   size_t count, loff_t *data)
{
	P_ADAPTER_T prAdapter;
	struct antenna_select_info *ant_sel;
	struct antenna_select_param params;
	unsigned char moving_avg_mode;
	unsigned short dwell;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	static const char * const opt[] = {
		"ant_query", "ant_avg", "ant_dwell" };
	UINT_32 u4BufLen = 0;
	UINT_32 u4CopySize = sizeof(aucProcBuf);
	int status;

	if (g_prGlueInfo_proc != NULL)
		prAdapter = g_prGlueInfo_proc->prAdapter;
	else
		return -EFAULT;

	kalMemSet(aucProcBuf, 0, u4CopySize);

	if (u4CopySize > count)
		u4CopySize = count;
	else
		u4CopySize = u4CopySize - 1;

	if (copy_from_user(aucProcBuf, buffer, u4CopySize)) {
		pr_err("error of copy from user\n");
		return -EFAULT;
	}


	aucProcBuf[u4CopySize] = '\0';
	ant_sel = &(prAdapter->rWifiVar.ant_select_info);

	if (kalStrnCmp(aucProcBuf, opt[0], strlen(opt[0])) == 0) {
		/* ant_query */
		status = sscanf(aucProcBuf +  strlen(opt[0]), "%u %u",
				&params.ant_num, &params.query_time);

		DBGLOG(INIT, INFO, "%s: %u %u\n",
		       opt[0],
		       params.ant_num, params.query_time);

		if (status != 2)
			return -EINVAL;

		rStatus = kalIoctl(g_prGlueInfo_proc, wlanoid_set_antenna_rssi,
				   (void *)&params,
				   sizeof(struct antenna_select_param),
				   FALSE, FALSE, FALSE, FALSE, &u4BufLen);
	} else if (kalStrnCmp(aucProcBuf, opt[1], strlen(opt[1])) == 0) {
		/* ant_avg */
		status = sscanf(aucProcBuf +  strlen(opt[1]), "%hhu",
				&moving_avg_mode);

		DBGLOG(INIT, INFO, "%s: %u\n",
		       opt[1], moving_avg_mode);

		if (status != 1)
			return -EINVAL;

		rStatus = kalIoctl(g_prGlueInfo_proc,
				   wlanoid_set_antenna_moving_avg_mode,
				   (void *)&moving_avg_mode,
				   sizeof(moving_avg_mode),
				   FALSE, FALSE, FALSE, FALSE, &u4BufLen);
	} else if (kalStrnCmp(aucProcBuf, opt[2], strlen(opt[2])) == 0) {
		/* ant_dwell */
		status = sscanf(aucProcBuf +  strlen(opt[2]), "%hu",
				&dwell);

		DBGLOG(INIT, INFO, "%s: %hu\n",
		       opt[2], dwell);

		if (status != 1)
			return -EINVAL;

		ant_sel->ch_dwell_time = dwell;
	} else {
		rStatus = WLAN_STATUS_NOT_SUPPORTED;
	}

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO, "failed to set antenna query params: %s\n",
		       aucProcBuf);
		return -EINVAL;
	}

	return count;
}

static const struct file_operations antenna_rssi_fops = {
	.owner = THIS_MODULE,
	.read = antenna_query_read,
	.write = antenna_query_write,
};
#endif

static ssize_t procVersionRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	UINT_32 u4CopySize;
	P_WIFI_VER_INFO_T wifiVer = &g_prGlueInfo_proc->prAdapter->rVerInfo;
	UINT_8 temp[64];
	/* if *f_pos > 0, it means has read successed last time, don't try again */
	if (*f_pos > 0)
		return 0;
	DBGLOG(INIT, INFO, "here4\n");
	kalSprintf(temp, "Firmware Version: 0x%x.%lx\n",
                wifiVer->u2FwOwnVersion>>8,
                wifiVer->u2FwOwnVersion & BITS(0, 7));

	u4CopySize = kalStrLen(temp);
	if (copy_to_user(buf, temp, u4CopySize)) {
		pr_err("copy to user failed\n");
		return -EFAULT;
	}
	*f_pos += u4CopySize;

	return (INT_32)u4CopySize;
}
static const struct file_operations version_fops = {
	.owner = THIS_MODULE,
	.read = procVersionRead,
};

static ssize_t procChipIDRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	UINT_32 u4CopySize;
	P_WIFI_VER_INFO_T wifiVer = &g_prGlueInfo_proc->prAdapter->rVerInfo;
	UINT_8 temp[25];
    /* if *f_pos > 0, it means has read successed last time, don't try again */
	if (*f_pos > 0)
		return 0;
	DBGLOG(INIT, INFO, "here\n");
	kalSprintf(temp, "Chip ID : 0x%04x\n", wifiVer->u2FwProductID);

	u4CopySize = kalStrLen(temp);
	if (copy_to_user(buf, temp, u4CopySize)) {
		pr_err("copy to user failed\n");
		return -EFAULT;
	}
	*f_pos += u4CopySize;

	return (INT_32)u4CopySize;
}
static const struct file_operations ChipID_fops = {
	.owner = THIS_MODULE,
	.read = procChipIDRead,
};

static ssize_t dtim_skip_count_read(struct file *filp,
				   char __user *buf,
				   size_t count, loff_t *f_pos)
{
	UINT_32 u4CopySize;
	P_ADAPTER_T prAdapter;
	int pos = 0;
	unsigned char dtim_skip_count = 0;

	if (g_prGlueInfo_proc != NULL)
		prAdapter = g_prGlueInfo_proc->prAdapter;
	else
		return -EFAULT;
	/* if *f_pos > 0, it means has read successed last time,
	 *  don't try again
	 */
	if (*f_pos > 0)
		return 0;

	dtim_skip_count = prAdapter->dtim_skip_count;
	pos += snprintf(aucProcBuf, sizeof(aucProcBuf),
			"DTIM Skip Count:%hhu\n",
			dtim_skip_count);

	u4CopySize = kalStrLen(aucProcBuf);
	if (copy_to_user(buf, aucProcBuf, u4CopySize)) {
		pr_err("copy to user failed\n");
		return -EFAULT;
	}
	*f_pos += u4CopySize;

	return (INT_32)u4CopySize;
}

static ssize_t dtim_skip_count_write(struct file *file,
				     const char __user *buffer,
				     size_t count, loff_t *data)
{
	P_ADAPTER_T prAdapter;
	unsigned char dtim_skip_count = 0;
	UINT_32 u4CopySize = sizeof(aucProcBuf);

	if (g_prGlueInfo_proc != NULL)
		prAdapter = g_prGlueInfo_proc->prAdapter;
	else
		return -EFAULT;

	kalMemSet(aucProcBuf, 0, u4CopySize);

	if (u4CopySize > count)
		u4CopySize = count;
	else
		u4CopySize = u4CopySize - 1;

	if (copy_from_user(aucProcBuf, buffer, u4CopySize)) {
		pr_err("error of copy from user\n");
		return -EFAULT;
	}

	aucProcBuf[u4CopySize] = '\0';

	if (sscanf(aucProcBuf, "%hhu", &dtim_skip_count) == 1) {
		if (dtim_skip_count > 6)
			return -EINVAL;
		prAdapter->dtim_skip_count = dtim_skip_count;
	} else {
		return -EINVAL;
	}

	return count;
}


static const struct file_operations dtim_ops = {
	.owner = THIS_MODULE,
	.read = dtim_skip_count_read,
	.write = dtim_skip_count_write,
};

INT_32 procInitFs(VOID)
{
	struct proc_dir_entry *prEntry;

	if (init_net.proc_net == (struct proc_dir_entry *)NULL) {
		pr_err("init proc fs fail: proc_net == NULL\n");
		return -ENOENT;
	}

	/*
	 * Directory: Root (/proc/net/wlan0)
	 */

	gprProcRoot = proc_mkdir(PROC_ROOT_NAME, init_net.proc_net);
	if (!gprProcRoot) {
		pr_err("gprProcRoot == NULL\n");
		return -ENOENT;
	}
	proc_set_user(gprProcRoot, KUIDT_INIT(PROC_UID_SHELL), KGIDT_INIT(PROC_GID_WIFI));

	prEntry = proc_create(PROC_DBG_LEVEL_NAME, 0664, gprProcRoot, &dbglevel_ops);
	if (prEntry == NULL) {
		pr_err("Unable to create /proc entry dbgLevel\n\r");
		return -1;
	}
	proc_set_user(prEntry, KUIDT_INIT(PROC_UID_SHELL), KGIDT_INIT(PROC_GID_WIFI));

	return 0;
}				/* end of procInitProcfs() */

INT_32 procUninitProcFs(VOID)
{
	remove_proc_entry(PROC_DBG_LEVEL_NAME, gprProcRoot);
	remove_proc_subtree(PROC_ROOT_NAME, init_net.proc_net);
	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function clean up a PROC fs created by procInitProcfs().
*
* \param[in] prDev      Pointer to the struct net_device.
* \param[in] pucDevName Pointer to the name of net_device.
*
* \return N/A
*/
/*----------------------------------------------------------------------------*/
INT_32 procRemoveProcfs(VOID)
{
	/* remove root directory (proc/net/wlan0) */
	/* remove_proc_entry(pucDevName, init_net.proc_net); */
	remove_proc_entry(PROC_WLAN_THERMO, gprProcRoot);
	remove_proc_entry(PROC_ROAM_PARAM, gprProcRoot);
	remove_proc_entry(PROC_COUNTRY, gprProcRoot);
	remove_proc_entry(PROC_WLAN_MCR, gprProcRoot);
	remove_proc_entry(PROC_VERSION, gprProcRoot);
	remove_proc_entry(PROC_RX_BA_WIN, gprProcRoot);
	remove_proc_entry(PROC_WLAN_CHIP_ID, gprProcRoot);
#ifdef CONFIG_MTK_WIFI_ANTENNA_SWITCH
	remove_proc_entry(PROC_ANTENNA_SELECT, gprProcRoot);
#endif
#ifdef CONFIG_MTK_WIFI_ANTENNA_SELECT
	remove_proc_entry(PROC_ANTENNA_RSSI, gprProcRoot);
#endif
	remove_proc_entry(PROC_DTIM, gprProcRoot);
#if CFG_SUPPORT_THERMO_THROTTLING
	g_prGlueInfo_proc = NULL;
#endif
	return 0;
}				/* end of procRemoveProcfs() */

INT_32 procCreateFsEntry(P_GLUE_INFO_T prGlueInfo)
{
	struct proc_dir_entry *prEntry;

	DBGLOG(INIT, TRACE, "[%s]\n", __func__);

#if CFG_SUPPORT_THERMO_THROTTLING
	g_prGlueInfo_proc = prGlueInfo;
#endif

	prGlueInfo->pProcRoot = gprProcRoot;

	prEntry = proc_create(PROC_WLAN_THERMO, 0664, gprProcRoot, &proc_fops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry\n\r");
		return -1;
	}

	prEntry = proc_create(PROC_ROAM_PARAM, 0664, gprProcRoot, &roam_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry\n\r");
		return -1;
	}

	prEntry = proc_create(PROC_COUNTRY, 0664, gprProcRoot, &country_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry\n\r");
		return -1;
	}

	prEntry = proc_create(PROC_WLAN_MCR, 0664, gprProcRoot, &mcr_fops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry\n\r");
		return -1;
	}

	prEntry = proc_create(PROC_VERSION, 0664, gprProcRoot, &version_fops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry\n\r");
		return -1;
	}

	prEntry = proc_create(PROC_WLAN_CHIP_ID, 0664, gprProcRoot, &ChipID_fops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry\n\r");
		return -1;
	}

	prEntry = proc_create(PROC_RX_BA_WIN, 0664, gprProcRoot, &rxba_fops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry\n\r");
		return -1;
	}
#ifdef CONFIG_MTK_WIFI_ANTENNA_SWITCH
	prEntry = proc_create(PROC_ANTENNA_SELECT, 0664, gprProcRoot, &antenna_select_fops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry\n\r");
		return -1;
	}
#endif
#ifdef CONFIG_MTK_WIFI_ANTENNA_SELECT
	prEntry = proc_create(PROC_ANTENNA_RSSI, 0664, gprProcRoot,
			      &antenna_rssi_fops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry\n\r");
		return -1;
	}
#endif
	prEntry = proc_create(PROC_DTIM, 0664, gprProcRoot, &dtim_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry\n\r");
		return -1;
	}

	proc_set_user(prEntry, KUIDT_INIT(PROC_UID_SHELL), KGIDT_INIT(PROC_GID_WIFI));
	return 0;
}

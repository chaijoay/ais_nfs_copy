///
///
/// FACILITY    : Copy/Merge files to various output directory and also do backup original file
///
/// FILE NAME   : nfs_copy.c
///
/// ABSTRACT    : Copy cdr from mtx to working directories
///
/// AUTHOR      : Thanakorn Nitipiromchai
///
/// CREATE DATE : 30-Apr-2019
///
/// CURRENT VERSION NO : 1.1.2
///
/// LAST RELEASE DATE  : 21-Nov-2019
///
/// MODIFICATION HISTORY :
///     1.0.0       30-Apr-2019     First Version
///     1.1.0       17-Sep-2019     flushes logState and not backup sync in case of self sync
///     1.1.1       19-Sep-2019     add copy mode feature to enable 1 to 1 copying (keep the same name for input and output)
///     1.1.2       21-Nov-2019     fix state file checking and able to uncompress input file then concat output
///
///
#define _XOPEN_SOURCE           700         // Required under GLIBC for nftw()
#define _POSIX_C_SOURCE         200809L
#define _XOPEN_SOURCE_EXTENDED  1

#include <unistd.h>
#include <sys/types.h>
#include <libgen.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <regex.h>
#include <ftw.h>
#include <time.h>


#include "minIni.h"
#include "procsig.h"
#include "nfs_copy.h"
#include "strlogutl.h"

// ===========================================================
// ----- MANDATORY log format for next processing of log -----
// 2017/05/04 10:44:52 [ERR] file /appl/testdir/nfs/md_fsyncnew/GGSN/GGSNNSNcwdc11_awngprs.200891_20170201.syn file size not ok
// 2017/05/03 14:30:56 [INF] file /appl/testdir/nfs/pgw/syn/4CGKKN1Z/4CGKKN1Z_SGW_awnlte.19450ss1_20170328.syn ok
// 2017/05/03 14:30:56 [INF] read 4CGKKN1Z_SGW_awnlte.194501_20170328.dat mtime=20170503140050 rec=30289 to AIS.4CGKKN1Z_AWNLTE.0503_143056_?
// 2017/05/04 10:53:00 [INF] read 3SGSNBPL1H_awngprsr.081701_20170503.dat mtime=20170503100050 rec=168 to AIS.3SGSNBPL1H_AWNGPR.0504_105259
// 2017/05/03 14:31:41 [INF] all above files dist to AIS.4CGKKN1Z_AWNLTE.0503_143056_0.n00 rec=216114
// 2017/05/04 10:53:00 [INF] all above files cat to AIS.3SGSNBPL1H_AWNGPR.0504_105259.n00 rec=88047
// -----------------------------------------------------------
// ===========================================================

unsigned long glByteCnt;
unsigned int  gnWrtRecCnt;
unsigned int  gnInpFileCntDay;
unsigned int  gnInpFileCntBat;
unsigned int  gnOutFileCntDay;
unsigned int  gnOutFileCntBat;
unsigned int  gnRunSeq;

char gszPrcType[SIZE_ITEM_T];
char gszAppName[SIZE_ITEM_S];
char gszIniFile[SIZE_FULL_NAME];

char gszIniParInput[E_NOF_PAR_INPUT][SIZE_ITEM_L];
char gszIniParSynInf[E_NOF_PAR_SYN_INF][SIZE_ITEM_L];
char gszIniParOutput[E_NOF_PAR_OUTPUT][SIZE_ITEM_L];
char gszIniParBackup[E_NOF_PAR_BACKUP][SIZE_ITEM_L];
char gszIniParCommon[E_NOF_PAR_COMMON][SIZE_ITEM_L];
char gszIniSubParItem[NOF_SUB_OUTDIR][E_NOF_PAR_SUBOUT][SIZE_ITEM_L];

//int  gnSynInfIndex[E_NOF_PAR_SYN_INF];
time_t gtOldFile;

int gnNofOutDir;
int gnSynCntAll;

static regex_t gsRxLeafPat;
static regex_t gsRxSynPat;
FILE   *gfpSnap;
FILE   *gfpState;
FILE   *gfpMerge;
int    gnRootDirLen;
int    gnCmdArg = E_NORMAL;
char   gszToday[SIZE_DATE_ONLY+1];

char   gszAlertFname1[SIZE_ITEM_L];
char   gszAlertFname2[SIZE_ITEM_L];
char   gszAlertStr2[SIZE_ITEM_L];
time_t gtLastProcTimeT;

time_t gtTimeCap1stSyn;
time_t gtTimeCapNewSyn;
time_t gtTimeCapValSyn;

const char gszIniSecName[E_NOF_SECTION][SIZE_ITEM_T] = {
    "INPUT",
    "SYNC_INFO",
    "OUTPUT",
    "BACKUP",
    "COMMON"
};

const char gszIniStrInput[E_NOF_PAR_INPUT][SIZE_ITEM_T] = {
    "SOURCE_TYPE",
    "ROOT_DIR_DATA",
    "ROOT_DIR_SYNC",
    "LEAF_DIR_SYNC",
    "SYNC_FNAME_PAT",
    "SYNC_FNAME_EXT",
    "DATA_FNAME_EXT",
    "DATA_FNAME_FROM",
    "DATA_SUB_DIR"
};

const char gszIniStrSynInf[E_NOF_PAR_SYN_INF][SIZE_ITEM_T] = {
    "SYN_INFO_FROM",
    "SYN_STABLE_SEC",
    "SYN_COL_DELIMIT",
    "SYN_NET_ID_COL",
    "SYN_NET_TYPE_COL",
    "SYN_DATANAME_COL",
    "SYN_DATASIZE_COL"
};

const char gszIniStrOutput[E_NOF_PAR_OUTPUT][SIZE_ITEM_T] = {
    "COPY_MODE",
    "MERGE_OUTPUT",
    "MEREGE_MAX_SIZE_MB",
    "FILE_PREFIX",
    "NETWORKMAP_FILE",
    "DECODER_PROGRAM"
};

const char gszIniStrBackup[E_NOF_PAR_BACKUP][SIZE_ITEM_T] = {
    "BACKUP_DATA",
    "BACKUP_SYNC",
    "BACKUP_DIR_DATA",
    "BACKUP_DIR_SYNC",
    "BACKUP_SUB_DATA",
    "BACKUP_SUB_SYNC"
};

const char gszIniStrCommon[E_NOF_PAR_COMMON][SIZE_ITEM_T] = {
    "TMP_DIR",
    "LOG_DIR",
    "LOG_LEVEL",
    "STATE_DIR",
    "KEEP_STATE_DAY",
    "REMOVE_DATA",
    "REMOVE_SYNC",
    "SLEEP_SECOND",
    "SKIP_OLD_FILE",
    "NO_SYN_ALERT_HOUR",
    "ALERT_LOG_DIR",
    "MERGE_LOG_DIR"
};

const char gszIniStrSubOutput[E_NOF_PAR_SUBOUT][SIZE_ITEM_T] = {
    "ROOT_DIR_DATA_",
    "ROOT_DIR_SYNC_",
    "LEAF_DIR_DATA_",
    "LEAF_DIR_SYNC_",
    "CREATE_SYNC_"
};

int main(int argc, char *argv[])
{

    char first_snap[SIZE_ITEM_L];
    char new_snap[SIZE_ITEM_L];
    char valid_snap[SIZE_ITEM_L];
    int retryBldSnap = 3;

    glByteCnt = 0;
    gnWrtRecCnt = 0;
    gnInpFileCntDay = 0;
    gnInpFileCntBat = 0;
    gnOutFileCntDay = 0;
    gnOutFileCntBat = 0;
    gnRunSeq = 0;
    gtTimeCap1stSyn = 0L;
    gtTimeCapNewSyn = 0L;
    gtTimeCapValSyn = 0L;
    gfpState = NULL;
    gfpMerge = NULL;

    // 1. read ini file
    if ( readConfig(argc, argv) != SUCCESS ) {
        return EXIT_FAILURE;
    }

    if ( procLock(gszAppName, E_CHK) != SUCCESS ) {
        fprintf(stderr, "another instance of %s is running\n", gszAppName);
        return EXIT_FAILURE;
    }

    if ( handleSignal() != SUCCESS ) {
        fprintf(stderr, "init handle signal failed: %s\n", getSigInfoStr());
        return EXIT_FAILURE;
    }

    if ( startLogging(gszIniParCommon[E_LOG_DIR], gszAppName, atoi(gszIniParCommon[E_LOG_LEVEL])) != SUCCESS ) {
        return EXIT_FAILURE;
    }

    if ( validateIni() == FAILED ) {
        return EXIT_FAILURE;
    }
    logHeader();

    memset(gszAlertStr2, 0x00, sizeof(gszAlertStr2));
    memset(gszToday, 0x00, sizeof(gszToday));
    strcpy(gszToday, getSysDTM(DTM_DATE_ONLY));
    gtLastProcTimeT = time(NULL);   // set last process time to start app time
    strcpy(gszAlertStr2, "no last file");

    // Main processing loop
    while ( TRUE ) {

        procLock(gszAppName, E_SET);
        
        if ( isTerminated() == TRUE ) {
            break;
        }

        // 2. list sync file -> build snapshot
        gtOldFile = (time_t)(atoi(gszIniParCommon[E_SKIP_OLD_FILE]) * 24 * 60 * 60);
        memset(first_snap, 0x00, sizeof(first_snap));
        sprintf(first_snap, "%s/%s.first", gszIniParCommon[E_TMP_DIR], gszAppName);

        if ( buildSnapFile(first_snap) != SUCCESS ) {
            if ( --retryBldSnap <= 0 ) {
                fprintf(stderr, "retry build snap exceeded\n");
                break;
            }
            sleep(10);
            continue;
        }
        retryBldSnap = 3;   // reset retry build snap
        if ( gnCmdArg == E_FIRST ) {
            printf("(first) snapshot file -> %s (%d)\n", first_snap, gnSynCntAll);
            break;
        }

        if ( gnSynCntAll > 0 ) {
            gtTimeCap1stSyn = 0L;
            // 3. verify old/processed sync files to be skipped
            memset(new_snap, 0x00, sizeof(new_snap));
            sprintf(new_snap, "%s/%s.new", gszIniParCommon[E_TMP_DIR], gszAppName);
            int file_cnt = chkSnapVsState(first_snap, new_snap);
            if ( file_cnt > 0 ) {

                if ( gnCmdArg == E_NEW ) {
                    printf("(new) snapshot file -> %s (%d)\n", new_snap, file_cnt);
                    break;
                }
                gtTimeCapNewSyn = 0L;
                gtLastProcTimeT = time(NULL);
                // 4. process snapshot -> verify sync files against data files
                memset(valid_snap, 0x00, sizeof(valid_snap));
                gnInpFileCntBat = 0;
                gnOutFileCntBat = 0;
                sprintf(valid_snap, "%s/%s.valid", gszIniParCommon[E_TMP_DIR], gszAppName);
                file_cnt = 0;
                if ( (file_cnt = chkSnapVsData(new_snap, valid_snap)) > 0 ) {
                    if ( gnCmdArg == E_VALID ) {
                        printf("(valid) snapshot file -> %s (%d)\n", valid_snap, file_cnt);
                        break;
                    }
                    gtTimeCapValSyn = 0L;
                    gtLastProcTimeT = time(NULL);
                    // 5. process data file -> copy/concat to destination
                    // 6. do backup processed file
                    // 7. write state file
                    if ( gszIniParOutput[E_COPY_MODE][0] == 'Y' ) {
                        one2oneCopy(valid_snap);
                    }
                    else {
                        wrtOutput(valid_snap);
                    }
                    if ( gnCmdArg == E_SINGLE ) {
                        printf("(single) snapshot file -> %s (%d) read from %d files, write to %d files\n", valid_snap, file_cnt, gnInpFileCntBat, gnOutFileCntBat);
                        break;
                    }
                }
                else {
                    writeLog(LOG_INF, "no sync file (valid state)");
                    ( gtTimeCapValSyn == 0L ? gtTimeCapValSyn = time(NULL) : gtTimeCapValSyn );
                }
            }
            else if ( file_cnt < 0 ) {
                break;  // There are some problem in reading state file
            }
            else {
                writeLog(LOG_INF, "no sync file (new state)");
                ( gtTimeCapNewSyn == 0L ? gtTimeCapNewSyn = time(NULL) : gtTimeCapNewSyn );
            }
        }
        else {
            ( gtTimeCap1stSyn == 0L ? gtTimeCap1stSyn = time(NULL) : gtTimeCap1stSyn );
        }

        if ( strcmp(gszToday, getSysDTM(DTM_DATE_ONLY)) ) {
            if ( gfpState != NULL ) {
                fclose(gfpState);
                gfpState = NULL;
            }
            if ( gfpMerge != NULL ) {
                fclose(gfpMerge);
                gfpMerge = NULL;
            }
            writeLog(LOG_INF, "total processed files for today in=%d out=%d", gnInpFileCntDay, gnOutFileCntDay);
            strcpy(gszToday, getSysDTM(DTM_DATE_ONLY));
            gnInpFileCntDay = 0;
            gnOutFileCntDay = 0;
            manageLogFile();
            clearOldState();
        }

        if ( isTerminated() == TRUE ) {
            break;
        }
        else {
            writeLog(LOG_INF, "sleep %s sec", gszIniParCommon[E_SLEEP_SEC]);
            sleep(atoi(gszIniParCommon[E_SLEEP_SEC]));
        }

        chkAlertNoSync();

    }
    if ( gfpState != NULL ) {
        fclose(gfpState);
        gfpState = NULL;
    }
    if ( gfpMerge != NULL ) {
        fclose(gfpMerge);
        gfpMerge = NULL;
    }
    procLock(gszAppName, E_CLR);
    writeLog(LOG_INF, "%s", getSigInfoStr());
    writeLog(LOG_INF, "------- %s %s process completely stop -------", _APP_NAME_, gszPrcType);
    stopLogging();

    return EXIT_SUCCESS;

}

int buildSnapFile(const char *snapfile)
{
    char cmd[SIZE_BUFF];
    gnSynCntAll = 0;
    gnRootDirLen = strlen(gszIniParInput[E_ROOT_DIRSYN]);

    // compile regular expression for leaf node directory
    if ( regcomp(&gsRxLeafPat, gszIniParInput[E_LEAF_DIRSYN], REG_NOSUB|REG_EXTENDED) ) {
        writeLog(LOG_ERR, "%s: invalid regular expression", gszIniParInput[E_LEAF_DIRSYN]);
        return FAILED;
    }

    // compile regular expression for sync file name
    if ( regcomp(&gsRxSynPat, gszIniParInput[E_SYN_FN_PAT], REG_NOSUB|REG_EXTENDED) ) {
        writeLog(LOG_ERR, "%s: invalid regular expression", gszIniParInput[E_SYN_FN_PAT]);
        regfree(&gsRxLeafPat);
        return FAILED;
    }

    // open first snap file for writing
    if ( (gfpSnap = fopen(snapfile, "w")) == NULL ) {
        writeLog(LOG_SYS, "unable to open %s for writing: %s\n", snapfile, strerror(errno));
        regfree(&gsRxLeafPat);
        regfree(&gsRxSynPat);
        return FAILED;
    }

    // recursively walk through directories and file and check matching
    // filename, leaf node directory name regarding compiled regex in _chkSynFile func
    writeLog(LOG_INF, "scaning sync file in directory %s", gszIniParInput[E_ROOT_DIRSYN]);
    if ( nftw(gszIniParInput[E_ROOT_DIRSYN], _chkSynFile, 32, FTW_DEPTH) ) {
        writeLog(LOG_SYS, "unable to read path %s: %s\n", gszIniParInput[E_ROOT_DIRSYN], strerror(errno));
        regfree(&gsRxLeafPat);
        regfree(&gsRxSynPat);
        fclose(gfpSnap);
        gfpSnap = NULL;
        return FAILED;
    }

    regfree(&gsRxLeafPat);
    regfree(&gsRxSynPat);
    fclose(gfpSnap);
    gfpSnap = NULL;

    // if there are sync files then sort the snap file
    if ( gnSynCntAll > 0 ) {
        memset(cmd, 0x00, sizeof(cmd));
        sprintf(cmd, "sort -T %s %s > %s.tmp 2>/dev/null", gszIniParCommon[E_TMP_DIR], snapfile, snapfile);
writeLog(LOG_DB3, "buildSnapFile cmd '%s'", cmd);
        if ( system(cmd) != SUCCESS ) {
            writeLog(LOG_SYS, "cannot sort file %s (%s)", snapfile, strerror(errno));
            sprintf(cmd, "rm -f %s %s.tmp", snapfile, snapfile);
            system(cmd);
            return FAILED;
        }
        sprintf(cmd, "mv %s.tmp %s 2>/dev/null", snapfile, snapfile);
writeLog(LOG_DB3, "buildSnapFile cmd '%s'", cmd);
        system(cmd);
    }
    else {
        writeLog(LOG_INF, "no sync file (first state)");
    }

    return SUCCESS;

}

int _chkSynFile(const char *fpath, const struct stat *info, int typeflag, struct FTW *ftwbuf)
{

    const char *fname = fpath + ftwbuf->base;
    char leafdir[SIZE_ITEM_L];
    time_t systime;

    if ( typeflag != FTW_F && typeflag != FTW_SL && typeflag != FTW_SLN )
        return 0;

    memset(leafdir, 0x00, SIZE_ITEM_L);
    if ( gnRootDirLen == 1 )    // in case root dir is only / (single slash)
        strncpy(leafdir, fpath+gnRootDirLen, (ftwbuf->base - gnRootDirLen));
    else
        strncpy(leafdir, fpath+gnRootDirLen+1, (ftwbuf->base - gnRootDirLen - 2));  // remove first and last slash eg. /root/l1/l2/file.syn -> l1/l2

writeLog(LOG_DB3, "_chkSynFile leafdir(%s), file(%s)", leafdir, fname);

    if ( regexec(&gsRxLeafPat, leafdir, 0, NULL, 0) )
        return 0;   // skip when leaf node is not matched

    if ( regexec(&gsRxSynPat, fname, 0, NULL, 0) )
        return 0;   // skip when file name is not matched

    if ( !(info->st_mode & (S_IRUSR|S_IRGRP|S_IROTH)) ) {
        writeLog(LOG_WRN, "no read permission for %s skipped", fname);
        return 0;
    }

    systime = time(NULL);
writeLog(LOG_DB2, "_chkSynFile pattern matched, check old file: file/conf %ld/%ld sec", (systime - info->st_mtime), gtOldFile);
    if ( systime - info->st_mtime > gtOldFile )
        return 0;   // skip when file is too old

    gnSynCntAll++;
    fprintf(gfpSnap, "%s|%s\n", leafdir, fname);    // first snap file output format -> <LEAF>|<SYNC_FILE>
    return 0;

}

int chkSnapVsState(const char *isnap, const char *osnap)
{
    char cmd[SIZE_BUFF];
    char tmp[SIZE_ITEM_L];
    FILE *fp = NULL;

    memset(tmp, 0x00, sizeof(tmp));
    memset(cmd, 0x00, sizeof(cmd));
    sprintf(tmp, "%s/%s_XXXXXX", gszIniParCommon[E_TMP_DIR], gszAppName);
    mkstemp(tmp);

	// close and flush current state file, in case it's opening
	if ( gfpState != NULL ) {
		fclose(gfpState);
		gfpState = NULL;
	}

    // create state file of current day just in case there is currently no any state file.
    sprintf(cmd, "touch %s/%s_%s%s", gszIniParCommon[E_STATE_DIR], gszAppName, gszToday, STATE_SUFF);
writeLog(LOG_DB3, "chkSnapVsState cmd '%s'", cmd);
    system(cmd);

    if ( chkStateAndConcat(tmp) == SUCCESS ) {
        // sort all state files (<APP_NAME>_<PROC_TYPE>_<YYYYMMDD>.proclist) to tmp file
        // state files format is <LEAF>|<SYNC_FILE>
        //sprintf(cmd, "sort -T %s %s/%s_*%s > %s.tmp 2>/dev/null", gszIniParCommon[E_TMP_DIR], gszIniParCommon[E_STATE_DIR], gszAppName, STATE_SUFF, tmp);
        sprintf(cmd, "sort -T %s %s > %s.tmp 2>/dev/null", gszIniParCommon[E_TMP_DIR], tmp, tmp);
writeLog(LOG_DB3, "chkSnapVsState cmd '%s'", cmd);
        system(cmd);
    }
    else {
        unlink(tmp);
        return FAILED;
    }
    // compare tmp file(sorted all state files) with sorted first_snap to get only unprocessed new files list
    sprintf(cmd, "comm -23 %s %s.tmp > %s 2>/dev/null", isnap, tmp, osnap);
writeLog(LOG_DB3, "chkSnapVsState cmd '%s'", cmd);
    system(cmd);
    sprintf(cmd, "rm -f %s %s.tmp", tmp, tmp);
writeLog(LOG_DB3, "chkSnapVsState cmd '%s'", cmd);
    system(cmd);

    // get record count from output file (osnap)
    sprintf(cmd, "cat %s | wc -l", osnap);
writeLog(LOG_DB3, "chkSnapVsState cmd '%s'", cmd);
    fp = popen(cmd, "r");
    fgets(tmp, sizeof(tmp), fp);
    pclose(fp);

    return atoi(tmp);

}

int chkSnapVsData(const char *isnap, const char *osnap)
{
    FILE *ifp = NULL;
    char cmd[SIZE_BUFF];
    char tmp[SIZE_ITEM_L];
    int result = 0;

    if ( gfpSnap != NULL ) {
        fclose(gfpSnap);
    }

    if ( (gfpSnap = fopen(osnap, "w")) == NULL ) {
        writeLog(LOG_SYS, "unable to open %s for writing: %s\n", osnap, strerror(errno));
        return FAILED;
    }
    if ( (ifp = fopen(isnap, "r")) == NULL ) {
        writeLog(LOG_SYS, "unable to open %s for reading: %s\n", isnap, strerror(errno));
        fclose(gfpSnap);
        gfpSnap = NULL;
        return FAILED;
    }

    //getTokenItem(const char *str, int fno, char sep, char *out)
    if ( strcmp(gszIniParInput[E_SRC_TYPE], _SELF_) == 0 )
        result = chkSnapVsData_Self(ifp, gfpSnap);
    else    // LEAF_IDENTICAL - or - LEAF_FROM_SYNC
        result = chkSnapVsData_VaryLeaf(ifp, gfpSnap);

    fclose(gfpSnap);
    gfpSnap = NULL;
    fclose(ifp);

    if ( result > 0 ) {
        memset(cmd, 0x00, sizeof(cmd));
        // verified_snap output format (valid snap)
        //  k1         k2     k3         4                5              6              7           8           9           10
        // <yyyymmdd>|<NEID>|<NET_TYPE>|<yyyymmddhhmmss>|<SYN_DAT_SIZE>|<DAT_DAT_SIZE>|<LEAF_SYNC>|<SYNC_FILE>|<LEAF_DATA>|<DATA_FILE>
        sprintf(cmd, "sort -T %s -k 1,1 -k 2,2 -k 3,3 %s > %s.sorting 2>/dev/null", gszIniParCommon[E_TMP_DIR], osnap, osnap);
writeLog(LOG_DB3, "chkSnapVsData cmd '%s'", cmd);
        system(cmd);
        sprintf(cmd, "mv %s.sorting %s", osnap, osnap);
writeLog(LOG_DB3, "chkSnapVsData cmd '%s'", cmd);
        system(cmd);

        memset(tmp, 0x00, sizeof(tmp));
        // get record count from output file (osnap)
        sprintf(cmd, "cat %s | wc -l", osnap);
writeLog(LOG_DB3, "chkSnapVsData cmd '%s'", cmd);
        ifp = popen(cmd, "r");
        fgets(tmp, sizeof(tmp), ifp);
        pclose(ifp);
        result = atoi(tmp);
    }

    return result;

}

int chkSnapVsData_Self(FILE *isnap, FILE *osnap)
{
    char netid[SIZE_ITEM_T], nettype[SIZE_ITEM_T];
    char line[SIZE_BUFF];
    char snap_item[2][SIZE_ITEM_L];
    char fname[SIZE_BUFF], yyyymmdd[SIZE_DATE_TIME+1];
    char syn_content[SIZE_ITEM_L];
    unsigned long fsize;
    int cnt = 0;

    // snap input format -> <LEAF>|<SYNC_FILE>
    memset(line, 0x00, sizeof(line));
    while ( fgets(line, sizeof(line), isnap) ) {

        memset(netid, 0x00, sizeof(netid));
        memset(nettype, 0x00, sizeof(nettype));
        memset(snap_item, 0x00, sizeof(snap_item));
        memset(syn_content, 0x00, sizeof(syn_content));

        getStrToken(snap_item, 2, line, "|");
        trimStr(snap_item[1]);
        strncpy(syn_content, snap_item[1], (strlen(snap_item[1]) - strlen(gszIniParInput[E_DAT_FN_EXT])));


        //getTokenItem(syn_content, gnSynInfIndex[E_SYN_NET_ID_COL], gszIniParSynInf[E_SYN_COL_DELI][0], netid);
        //getTokenItem(syn_content, gnSynInfIndex[E_SYN_NET_TYPE_COL], gszIniParSynInf[E_SYN_COL_DELI][0], nettype);
        getItemFromStr(syn_content, gszIniParSynInf[E_SYN_NET_ID_COL], gszIniParSynInf[E_SYN_COL_DELI], netid);
        getItemFromStr(syn_content, gszIniParSynInf[E_SYN_NET_TYPE_COL], gszIniParSynInf[E_SYN_COL_DELI], nettype);

writeLog(LOG_DB3, "chkSnapVsData_Self SELF syn_cont(%s)(%s) (%s)netid(%s), (%s)nettype(%s)",
        syn_content, gszIniParSynInf[E_SYN_COL_DELI], gszIniParSynInf[E_SYN_NET_ID_COL],
        netid, gszIniParSynInf[E_SYN_NET_TYPE_COL], nettype);

        memset(fname, 0x00, sizeof(fname));
        memset(yyyymmdd, 0x00, sizeof(yyyymmdd));
        fsize = 0;
        sprintf(fname, "%s/%s/%s", gszIniParInput[E_ROOT_DIRSYN], snap_item[0], snap_item[1]);
        if ( isSynStable(fname, atoi(gszIniParSynInf[E_SYN_STABLE_SEC]), yyyymmdd, &fsize) == TRUE ) {
            // verified_snap output format (valid snap)
            //  k1         k2     k3         4                5              6              7           8           9           10
            // <yyyymmdd>|<NEID>|<NET_TYPE>|<yyyymmddhhmmss>|<SYN_DAT_SIZE>|<DAT_DAT_SIZE>|<LEAF_SYNC>|<SYNC_FILE>|<LEAF_DATA>|<DATA_FILE>
            fprintf(osnap, "%.8s|%s|%s|%s|%ld|%ld|%s|%s|%s|%s\n", yyyymmdd, netid, nettype, yyyymmdd, fsize, fsize, snap_item[0], snap_item[1], snap_item[0], snap_item[1]);
            cnt++;
        }

    }
    return cnt;

}

int chkSnapVsData_VaryLeaf(FILE *isnap, FILE *osnap)
{

    char netid[SIZE_ITEM_T], nettype[SIZE_ITEM_T], datname[SIZE_ITEM_L];
    char line[SIZE_BUFF];
    char snap_item[2][SIZE_ITEM_L];
    char syn_fname[SIZE_ITEM_L], yyyymmdd[SIZE_DATE_TIME+1];
    char dat_fname[SIZE_ITEM_L];
    char syn_content[SIZE_BUFF];
    char dat_leaf[SIZE_ITEM_L];
    char snp_itm[NOF_VSNAP][SIZE_ITEM_L];
    unsigned long dfsize;
    int cnt = 0;

    // snap input format -> <LEAF>|<SYNC_FILE>
    memset(line, 0x00, sizeof(line));
    while ( fgets(line, sizeof(line), isnap) ) {

        memset(netid, 0x00, sizeof(netid));
        memset(nettype, 0x00, sizeof(nettype));
        memset(datname, 0x00, sizeof(datname));
        memset(snap_item, 0x00, sizeof(snap_item));
        memset(syn_content, 0x00, sizeof(syn_content));
        memset(syn_fname, 0x00, sizeof(syn_fname));

        getStrToken(snap_item, 2, line, "|");
        trimStr(snap_item[1]);

        if ( strcmp(gszIniParSynInf[E_SYN_INFFROM], _CONTENT_) == 0 ) {
            sprintf(syn_fname, "%s/%s/%s", gszIniParInput[E_ROOT_DIRSYN], snap_item[0], snap_item[1]);
writeLog(LOG_DB3, "chkSnapVsData_VaryLeaf reading '%s'", syn_fname);
            FILE *fp = NULL;
            if ( (fp = fopen(syn_fname, "r")) == NULL ) {
                writeLog(LOG_ERR, "cannot open sync file %s (%s)", syn_fname, strerror(errno));
                continue;
            }
            if ( !fgets(syn_content, sizeof(syn_content), fp) ) {
                writeLog(LOG_ERR, "cannot read sync content %s (%s)", syn_fname, strerror(errno));
                fclose(fp);
                continue;
            }
            fclose(fp);
        }
        else {  // _SYNCNAME_
            strcpy(syn_content, snap_item[1]);
        }
        memset(snp_itm, 0x00, sizeof(snp_itm));

        //getTokenItem(syn_content, gnSynInfIndex[E_SYN_NET_ID_COL], gszIniParSynInf[E_SYN_COL_DELI][0], snp_itm[E_NEID]);
        //getTokenItem(syn_content, gnSynInfIndex[E_SYN_NET_TYPE_COL], gszIniParSynInf[E_SYN_COL_DELI][0], snp_itm[E_NETTYPE]);
        //getTokenItem(syn_content, gnSynInfIndex[E_SYN_DATSIZE_COL], gszIniParSynInf[E_SYN_COL_DELI][0], snp_itm[E_SYN_DATSIZE]);
        getItemFromStr(syn_content, gszIniParSynInf[E_SYN_NET_ID_COL], gszIniParSynInf[E_SYN_COL_DELI], snp_itm[E_NEID]);
        getItemFromStr(syn_content, gszIniParSynInf[E_SYN_NET_TYPE_COL], gszIniParSynInf[E_SYN_COL_DELI], snp_itm[E_NETTYPE]);
        getItemFromStr(syn_content, gszIniParSynInf[E_SYN_DATSIZE_COL], gszIniParSynInf[E_SYN_COL_DELI], snp_itm[E_SYN_DATSIZE]);

writeLog(LOG_DB3, "chkSnapVsData_VaryLeaf syn_cont(%s)(%s) (%s)netid(%s), (%s)nettype(%s), (%s)fsize(%s)",
                  syn_content, gszIniParSynInf[E_SYN_COL_DELI], gszIniParSynInf[E_SYN_NET_ID_COL],
                  snp_itm[E_NEID], gszIniParSynInf[E_SYN_NET_TYPE_COL], snp_itm[E_NETTYPE], gszIniParSynInf[E_SYN_DATSIZE_COL], snp_itm[E_SYN_DATSIZE]);

        if ( strcmp(gszIniParInput[E_DAT_FN_FROM], _SYN_DATANAME_COL_) == 0 ) {
            //getTokenItem(syn_content, gnSynInfIndex[E_SYN_DATNAME_COL], gszIniParSynInf[E_SYN_COL_DELI][0], datname);
            getItemFromStr(syn_content, gszIniParSynInf[E_SYN_DATNAME_COL], gszIniParSynInf[E_SYN_COL_DELI], datname);
writeLog(LOG_DB3, "chkSnapVsData_VaryLeaf DatName from syn content %s", datname);
        }
        else {  // data file name from sync file name ( by replace sync suffix with data suffix )
            strcpy(datname, snap_item[1]);
            strReplaceLast(datname, gszIniParInput[E_SYN_FN_EXT], gszIniParInput[E_DAT_FN_EXT]);
writeLog(LOG_DB3, "chkSnapVsData_VaryLeaf DatName from syn name %s", datname, gszIniParInput[E_SYN_FN_EXT], gszIniParInput[E_DAT_FN_EXT]);
        }

        memset(dat_leaf, 0x00, sizeof(dat_leaf));
        if ( strcmp(gszIniParInput[E_SRC_TYPE], _LEAF_SYNC_) == 0 ) {
            dfsize = 0;
            char cur_dtm[SIZE_DATE_ONLY+1];
            memset(cur_dtm, 0x00, sizeof(cur_dtm));
            memset(yyyymmdd, 0x00, sizeof(yyyymmdd));
            strcpy(cur_dtm, getSysDTM(DTM_DATE_ONLY));

            // call this func to get information from sync file
            isSynStable(syn_fname, atoi(gszIniParSynInf[E_SYN_STABLE_SEC]), yyyymmdd, &dfsize);
            strncpy(snp_itm[E_YMD8], yyyymmdd, 8);
            prepLeafDir(gszIniParSynInf[E_DAT_SUBDIR], snp_itm, cur_dtm, dat_leaf);
writeLog(LOG_DB3, "chkSnapVsData_VaryLeaf LEAF_SYNC data node %s", dat_leaf);
        }
        if ( strcmp(gszIniParInput[E_SRC_TYPE], _LEAF_IDEN_) == 0 ) {  // LEAF_IDENTICAL
            strcpy(dat_leaf, snap_item[0]);
writeLog(LOG_DB3, "chkSnapVsData_VaryLeaf LEAF_IDEN data node %s", dat_leaf);
        }

        memset(dat_fname, 0x00, sizeof(dat_fname));
        memset(yyyymmdd, 0x00, sizeof(yyyymmdd));
        dfsize = 0;
        sprintf(dat_fname, "%s/%s/%s", gszIniParInput[E_ROOT_DIRDAT], dat_leaf, datname);
        if ( isSynStable(dat_fname, atoi(gszIniParSynInf[E_SYN_STABLE_SEC]), yyyymmdd, &dfsize) == TRUE ) {
            if ( strcmp(gszIniParSynInf[E_SYN_INFFROM], _SYNCNAME_) == 0 ) {
                sprintf(snp_itm[E_SYN_DATSIZE], "%ld", dfsize);     // so filesize in sync content when sync info is from filename, so that filesize is from actual data filesize
            }
            // verified_snap output format (valid snap)
            //  k1         k2     k3         4                5              6              7           8           9           10
            // <yyyymmdd>|<NEID>|<NET_TYPE>|<yyyymmddhhmmss>|<SYN_DAT_SIZE>|<DAT_DAT_SIZE>|<LEAF_SYNC>|<SYNC_FILE>|<LEAF_DATA>|<DATA_FILE>
            fprintf(osnap, "%.8s|%s|%s|%s|%s|%ld|%s|%s|%s|%s\n", yyyymmdd, snp_itm[E_NEID], snp_itm[E_NETTYPE], yyyymmdd, snp_itm[E_SYN_DATSIZE], dfsize, snap_item[0], snap_item[1], dat_leaf, datname);
            cnt++;
        }
    }
    return cnt;

}

int isSynStable(const char *fname, int stable_sec, char *mod_ymd, unsigned long *fsize)
{

    struct stat stat_buf;
    time_t  time_diff = 0;
    time_t  systime = 0;
    unsigned long size = 0;
    int result = FALSE;

    strcpy(mod_ymd, getFileTimeM(fname, "%Y%m%d%H%M%S"));
    memset(&stat_buf, 0x00, sizeof(stat_buf));
    if ( !lstat(fname, &stat_buf) ) {
        systime = time(NULL);
        time_diff = systime - stat_buf.st_mtime;
        size = stat_buf.st_size;
        *fsize = size;
        if ( time_diff >= stable_sec ) {
            result = TRUE;
        }
    }
writeLog(LOG_DB3, "isSynStable %s (file/conf %ld/%d sec) size %ld byte %s", fname, time_diff, stable_sec, size, mod_ymd);
    return result;

}

// all_snap -> filtered_snap(old/dup) -> verified_snap(size,readiness:sync-data)
//
// all_snap (both first sanp and new snap)
// <LEAF>|<SYNC_FILE>
//
// verified_snap (valid snap)
//  k1         k2     k3         4                5              6              7           8           9           10
// <yyyymmdd>|<NEID>|<NET_TYPE>|<yyyymmddhhmmss>|<SYN_DAT_SIZE>|<DAT_DAT_SIZE>|<LEAF_SYNC>|<SYNC_FILE>|<LEAF_DATA>|<DATA_FILE>
//
// Data/Sync output file name <NEID>_<SYN_MMDD>_<$SYS_MMDD_HHMMSS>.<dat|syn>
//
// Sync format
// <NEID>|<CDR_FEED_TYPE(from network map)>|<MERGE_DATA_FILE>|<MERGE_FILE_SIZE>|<LOC_LEAF_NODE>|<SYS_DATE_FULL_FORMAT>|<PROC_TYPE>
//
int wrtOutput(const char *snapfile)
{
    char snp_line[SIZE_BUFF];
    char snp_inf[NOF_VSNAP][SIZE_ITEM_L];
    char full_inp_datfile[SIZE_ITEM_L];
    char full_inp_decoded[SIZE_ITEM_L];
    char dat_line[SIZE_BUFF];
    char cat_file[SIZE_ITEM_L];
    char full_tmp_catfile[SIZE_ITEM_L];
    char tmp_synfile[SIZE_ITEM_L];
    char curr_node[SIZE_ITEM_S], prev_node[SIZE_ITEM_S];
    char datetime[SIZE_DATE_TIME+1];
    char mmdd[SIZE_DATE_ONLY+1];
    unsigned long rdrec_cnt;
    unsigned long max_catsize = atol(gszIniParOutput[E_MRG_MAX_SIZE_MB]) * MBYTE;
    int exit_loop = 0;

    FILE *ifp = NULL, *rfp = NULL, *wfp = NULL;

    // open verified snap file for read
    if ( (ifp = fopen(snapfile, "r")) == NULL ) {
        writeLog(LOG_ERR, "cannot open %s for reading (%s)", snapfile, strerror(errno));
        return FAILED;
    }
    else {
        memset(curr_node, 0x00, sizeof(curr_node));
        memset(prev_node, 0x00, sizeof(prev_node));
        gnInpFileCntBat = 0;
        gnOutFileCntBat = 0;

        while ( fgets(snp_line, sizeof(snp_line), ifp) ) {

            char ori_syn[SIZE_ITEM_L];
            memset(ori_syn, 0x00, sizeof(ori_syn));
            memset(snp_inf, 0x00, sizeof(snp_inf));
            trimStr(snp_line);

writeLog(LOG_DB3, "wrtOutput snp_line %s", snp_line);

            getStrToken(snp_inf, NOF_VSNAP, snp_line, "|");

writeLog(LOG_DB3, "wrtOutput snp_inf %s|%s|%s|%s|%s|%s|%s|%s|%s|%s",
        snp_inf[0], snp_inf[1], snp_inf[2], snp_inf[3], snp_inf[4],
        snp_inf[5], snp_inf[6], snp_inf[7], snp_inf[8], snp_inf[9]);

            sprintf(ori_syn, "%s/%s/%s", gszIniParInput[E_ROOT_DIRSYN], snp_inf[E_LEAF_SYN], snp_inf[E_SYN_FILE]);
writeLog(LOG_DB3, "wrtOutput ori_syn '%s'", ori_syn);
            if ( strcmp(snp_inf[E_SYN_DATSIZE], snp_inf[E_DAT_DATSIZE]) != 0 || strcmp(snp_inf[E_SYN_DATSIZE], "0") == 0 ) {
                writeLog(LOG_ERR, "file %s file size not ok (syn[%s]|dat[%s])", ori_syn, snp_inf[E_SYN_DATSIZE], snp_inf[E_DAT_DATSIZE]);
                char err_syn[SIZE_ITEM_L];
                memset(err_syn, 0x00, sizeof(err_syn));
                sprintf(err_syn, "%s.ERR", ori_syn);
                if ( rename(ori_syn, err_syn) != SUCCESS ) {
                    writeLog(LOG_WRN, "cannot rename %s to ERR (%s)", ori_syn, strerror(errno));
                }
                logState(snp_inf[E_LEAF_SYN], snp_inf[E_SYN_FILE]);
                continue;
            }
            writeLog(LOG_INF, "file %s ok", ori_syn);
            // check file size and node to be close file
            // if node changed, merge output is set to No or cat size exceeded -> close output file and create sync file
            sprintf(curr_node, "%s|%s", snp_inf[E_YMD8], snp_inf[E_NEID]);
writeLog(LOG_DB3, "wrtOutput curr_node(%s) prev_node(%s) glByteCnt/MAX_SIZE(%ld/%ld)", curr_node, prev_node, glByteCnt, max_catsize);
            if ( (strcmp(curr_node, prev_node) != 0) || prev_node[0] == '\0' ||
                 gszIniParOutput[E_MRG_OUTPUT][0] == 'N' || glByteCnt >= max_catsize ) {
                if ( wfp != NULL ) {
                    fclose(wfp);
                    wfp = NULL;
                }
                if ( prev_node[0] != '\0' ) {
                    relocDataAndGenSync(full_tmp_catfile, tmp_synfile, snp_inf, NOF_VSNAP);
                }
                memset(datetime, 0x00, sizeof(datetime));
                memset(full_tmp_catfile, 0x00, sizeof(full_tmp_catfile));
                memset(cat_file, 0x00, sizeof(cat_file));
                memset(mmdd, 0x00, sizeof(mmdd));

                // prepare new concat file name
                ( ++gnRunSeq >= MAX_RUNNING_SEQ ? gnRunSeq = 1 : gnRunSeq );
                strcpy(datetime, getSysDTM(DTM_DATE_TIME));
                strncpy(mmdd, snp_inf[E_YMD8]+4, 4);
                //<NEID>_<$SYN_mmdd>_<$SYS_yyyymmddhhmmss>_<xxx>.<$ext>
                sprintf(tmp_synfile, "%s%s_%s_%s_%03d%s", gszIniParOutput[E_FILE_PREFIX], snp_inf[E_NEID], mmdd, datetime, gnRunSeq, OUT_SYNC_EXT);
                sprintf(cat_file,    "%s%s_%s_%s_%03d%s", gszIniParOutput[E_FILE_PREFIX], snp_inf[E_NEID], mmdd, datetime, gnRunSeq, OUT_DATA_EXT);
                sprintf(full_tmp_catfile, "%s/%s", gszIniParCommon[E_TMP_DIR], cat_file);
writeLog(LOG_DB3, "wrtOutput tmp_synfile(%s) cat_file(%s) full_tmp_catfile(%s)", tmp_synfile, cat_file, full_tmp_catfile);
                strcpy(prev_node, curr_node);
                glByteCnt = 0;
                gnWrtRecCnt = 0;

            }

            memset(full_inp_decoded, 0x00, sizeof(full_inp_decoded));
            memset(full_inp_datfile, 0x00, sizeof(full_inp_datfile));
            sprintf(full_inp_datfile, "%s/%s/%s", gszIniParInput[E_ROOT_DIRDAT], snp_inf[E_LEAF_DAT], snp_inf[E_DAT_FILE]);
writeLog(LOG_DBG, "wrtOutput reading %s to %s", full_inp_datfile, cat_file);

            if ( extDecoder(full_inp_datfile, full_inp_decoded) != SUCCESS ) {
                logState(snp_inf[E_LEAF_SYN], snp_inf[E_SYN_FILE]);
                continue;
            }

            if ( (rfp = fopen(full_inp_decoded, "r")) == NULL ) {
                writeLog(LOG_ERR, "cannot open %s for reading (%s)", full_inp_datfile, strerror(errno));
                chkToDelete(full_inp_decoded, full_inp_datfile);
                continue;
            }
            else {
                memset(dat_line, 0x00, sizeof(dat_line));
                rdrec_cnt = 0;
                while ( fgets(dat_line, sizeof(dat_line), rfp) ) {        // loop through input data record for writing 1-1/merged output
                    if ( wfp == NULL ) {
                        if ( (wfp = fopen(full_tmp_catfile, "a")) == NULL ) {
                            writeLog(LOG_ERR, "cannot open %s for writing (%s)", cat_file, strerror(errno));
                            exit_loop = 1;
                            break;
                        }
                        glByteCnt += fprintf(wfp, "%s", dat_line);
                        rdrec_cnt++;
                    }
                    else {
                        glByteCnt += fprintf(wfp, "%s", dat_line);
                        rdrec_cnt++;
                    }
                }
                fclose(rfp);
                rfp = NULL;

                if ( exit_loop )
                    break;

                // keep state of which file has been processed
                // sort all state files (<APP_NAME>_<PROC_TYPE>_<YYYYMMDD>.proclist) to tmp file
                // state files format is <LEAF>|<SYNC_FILE>
                logState(snp_inf[E_LEAF_SYN], snp_inf[E_SYN_FILE]);

                // log list of merged files
                logMergeList("Read", full_inp_datfile);

                // do backup and keep state here
                doBackup(snp_inf);

                writeLog(LOG_INF, "read %s mtime=%s rec=%ld to %s", snp_inf[E_DAT_FILE], snp_inf[E_YMD14], rdrec_cnt, cat_file);
                sprintf(gszAlertStr2, "%s|%s|%ld", snp_inf[E_YMD14], snp_inf[E_DAT_FILE], rdrec_cnt);
                
                chkToDelete(full_inp_decoded, full_inp_datfile);
                gnWrtRecCnt += rdrec_cnt;
                gnInpFileCntBat++;
                gnInpFileCntDay++;
            }
            if ( isTerminated() == TRUE ) {
                break;
            }
        }
        fclose(ifp);
        if ( wfp != NULL ) {
            fclose(wfp);
            wfp = NULL;
            relocDataAndGenSync(full_tmp_catfile, tmp_synfile, snp_inf, NOF_VSNAP);
        }
        writeLog(LOG_INF, "total processed files for this round in=%d out=%d", gnInpFileCntBat, gnOutFileCntBat);
        gtLastProcTimeT = time(NULL);   // reset last process time;
    }

    return SUCCESS;

}

int relocDataAndGenSync(const char *full_catfile, const char *synfile, char snp_inf[][SIZE_ITEM_L], int snpsize)
{
    char cmd[SIZE_BUFF];
    struct stat stat_buf;
    char cdrfeedtype[SIZE_ITEM_T];
    char fdatetime[SIZE_DATE_TIME_FULL+1];
    char datetime[SIZE_DATE_ONLY+1];
    char synleaf[SIZE_ITEM_L];
    char datleaf[SIZE_ITEM_L];
    char sync_content[SIZE_BUFF];
    char datfile[SIZE_ITEM_L];

    unsigned long fsize = 0;

    memset(cdrfeedtype, 0x00, sizeof(cdrfeedtype));
    memset(&stat_buf, 0x00, sizeof(stat_buf));
    memset(datetime, 0x00, sizeof(datetime));
    memset(fdatetime, 0x00, sizeof(fdatetime));
    memset(datfile, 0x00, sizeof(datfile));
    memset(cmd, 0x00, sizeof(cmd));

    mapNetType(snp_inf[E_NETTYPE], cdrfeedtype);
    if ( lstat(full_catfile, &stat_buf) ) {
        writeLog(LOG_SYS, "cannot get file info %s (%s)", full_catfile, strerror(errno));
        return FAILED;
    }
    fsize = (unsigned long)stat_buf.st_size;

    strcpy(fdatetime, getSysDTM(DTM_DATE_TIME_FULL));
    strcpy(datetime, getSysDTM(DTM_DATE_ONLY));
    strcpy(datfile, basename((char*)full_catfile));

    logMergeList("Write", datfile);

    int i;
    for ( i = 0; i < gnNofOutDir; i++ ) {   // loop through total number of output directory

        memset(sync_content, 0x00, sizeof(sync_content));
        memset(synleaf, 0x00, sizeof(synleaf));
        memset(datleaf, 0x00, sizeof(datleaf));

        if ( gszIniSubParItem[i][E_LEAF_DIRDAT_][0] != '\0' ) {
            prepLeafDir(gszIniSubParItem[i][E_LEAF_DIRDAT_], snp_inf, datetime, datleaf);
writeLog(LOG_DB3, "relocDataAndGenSync prepLeafDir Dat '%s' -> '%s'", gszIniSubParItem[i][E_LEAF_DIRDAT_], datleaf);
        }

        // sync format
        // mtxid|cdrfeedtype|basename(sDatFname)|filesize|file_location|sCurDate|gbl_ProcType
        sprintf(sync_content, "%s|%s|%s|%ld|%s%s|%s|%s", snp_inf[E_NEID], cdrfeedtype, datfile, fsize, ( *datleaf ? "/" : "" ), datleaf, fdatetime, gszPrcType);
writeLog(LOG_DB3, "relocDataAndGenSync sync_content '%s'", sync_content);
        sprintf(cmd, "mkdir -p %s/%s 2>/dev/null", gszIniSubParItem[i][E_ROOT_DIRDAT_], datleaf);
writeLog(LOG_DB3, "relocDataAndGenSync cmd '%s'", cmd);
        system(cmd);
        
        sprintf(cmd, "chmod 755 %s/%s", gszIniSubParItem[i][E_ROOT_DIRDAT_], datleaf);
        system(cmd);
        
        sprintf(cmd, "cp -p %s %s/%s 2>/dev/null", full_catfile, gszIniSubParItem[i][E_ROOT_DIRDAT_], datleaf);
writeLog(LOG_DB3, "relocDataAndGenSync cmd '%s'", cmd);
        system(cmd);

        char dat[SIZE_ITEM_L];
        memset(dat, 0x00, sizeof(dat));
        sprintf(dat, "%s/%s/%s", gszIniSubParItem[i][E_ROOT_DIRDAT_], datleaf, datfile);
        chmod(dat, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
writeLog(LOG_DB3, "relocDataAndGenSync out dat '%s'", dat);
        
        if ( gszIniSubParItem[i][E_CREATE_SYN_][0] == 'Y' ) {
            char syn[SIZE_ITEM_L];
            FILE *sfp = NULL;
            memset(syn, 0x00, sizeof(syn));

            if ( gszIniSubParItem[i][E_LEAF_DIRSYN_][0] != '\0' ) {
                prepLeafDir(gszIniSubParItem[i][E_LEAF_DIRSYN_], snp_inf, datetime, synleaf);
writeLog(LOG_DB3, "relocDataAndGenSync prepLeafDir Syn '%s' -> '%s'", gszIniSubParItem[i][E_LEAF_DIRSYN_], synleaf);
            }
            sprintf(cmd, "mkdir -p %s/%s 2>/dev/null", gszIniSubParItem[i][E_ROOT_DIRSYN_], synleaf);
writeLog(LOG_DB3, "relocDataAndGenSync cmd '%s'", cmd);
            system(cmd);
            
            sprintf(cmd, "chmod 755 %s/%s", gszIniSubParItem[i][E_ROOT_DIRSYN_], synleaf);
            system(cmd);

            sprintf(syn, "%s/%s/%s", gszIniSubParItem[i][E_ROOT_DIRSYN_], synleaf, synfile);
writeLog(LOG_DB3, "relocDataAndGenSync out syn '%s'", syn);
            if ( (sfp = fopen(syn, "w")) != NULL ) {
                fprintf(sfp, "%s\n", sync_content);
                fclose(sfp);
                sfp = NULL;
            }
            chmod(syn, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
        }
    }

    writeLog(LOG_INF, "all above files cat to %s rec=%d", datfile, gnWrtRecCnt);
    sprintf(cmd, "rm -f %s", full_catfile);
writeLog(LOG_DB3, "relocDataAndGenSync cmd '%s'", cmd);
    system(cmd);
    gnOutFileCntBat++;
    gnOutFileCntDay++;

    return SUCCESS;

}

int one2oneCopy(const char *snapfile)
{
    char snp_line[SIZE_BUFF];
    char snp_inf[NOF_VSNAP][SIZE_ITEM_L];
    char ori_syn[SIZE_ITEM_L];
    char ori_dat[SIZE_ITEM_L];
    char synleaf[SIZE_ITEM_L];
    char datleaf[SIZE_ITEM_L];
    char ocksum1[SIZE_ITEM_S];
    char ocksum2[SIZE_ITEM_S];
    char dest_dat[SIZE_ITEM_L];
    char dest_syn[SIZE_ITEM_L];
    char datetime[SIZE_DATE_ONLY+1];
    char cmd[SIZE_BUFF];
    int  retry;
    FILE *ifp = NULL;

    strcpy(datetime, getSysDTM(DTM_DATE_ONLY));
    // open verified snap file for read
    if ( (ifp = fopen(snapfile, "r")) == NULL ) {
        writeLog(LOG_ERR, "cannot open %s for reading (%s)", snapfile, strerror(errno));
        return FAILED;
    }
    else {
        gnInpFileCntBat = 0;
        gnOutFileCntBat = 0;

        while ( fgets(snp_line, sizeof(snp_line), ifp) ) {
            
            memset(ori_syn, 0x00, sizeof(ori_syn));
            memset(ori_dat, 0x00, sizeof(ori_dat));
            memset(snp_inf, 0x00, sizeof(snp_inf));
            memset(ocksum1, 0x00, sizeof(ocksum1));

            trimStr(snp_line);

writeLog(LOG_DB3, "one2oneCopy snp_line %s", snp_line);

            getStrToken(snp_inf, NOF_VSNAP, snp_line, "|");

writeLog(LOG_DB3, "one2oneCopy snp_inf  %s|%s|%s|%s|%s|%s|%s|%s|%s|%s",
        snp_inf[0], snp_inf[1], snp_inf[2], snp_inf[3], snp_inf[4],
        snp_inf[5], snp_inf[6], snp_inf[7], snp_inf[8], snp_inf[9]);

            sprintf(ori_syn, "%s/%s/%s", gszIniParInput[E_ROOT_DIRSYN], snp_inf[E_LEAF_SYN], snp_inf[E_SYN_FILE]);
            sprintf(ori_dat, "%s/%s/%s", gszIniParInput[E_ROOT_DIRDAT], snp_inf[E_LEAF_DAT], snp_inf[E_DAT_FILE]);

writeLog(LOG_DB3, "one2oneCopy ori_syn '%s'", ori_syn);
writeLog(LOG_DB3, "one2oneCopy ori_dat '%s'", ori_dat);
            if ( strcmp(snp_inf[E_SYN_DATSIZE], snp_inf[E_DAT_DATSIZE]) != 0 || strcmp(snp_inf[E_SYN_DATSIZE], "0") == 0 ) {
                writeLog(LOG_ERR, "file %s file size not ok (syn[%s]|dat[%s])", ori_syn, snp_inf[E_SYN_DATSIZE], snp_inf[E_DAT_DATSIZE]);
                char err_syn[SIZE_ITEM_L];
                memset(err_syn, 0x00, sizeof(err_syn));
                sprintf(err_syn, "%s.ERR", ori_syn);
                if ( rename(ori_syn, err_syn) != SUCCESS ) {
                    writeLog(LOG_WRN, "cannot rename %s to ERR (%s)", ori_syn, strerror(errno));
                }
                logState(snp_inf[E_LEAF_SYN], snp_inf[E_SYN_FILE]);
                continue;
            }
            writeLog(LOG_INF, "file %s ok", ori_syn);
            
            getCksumStr(ori_dat, ocksum1, sizeof(ocksum1));
            trimStr(ocksum1);
            
            // start copy file from source to (multi) destination
            int i;
            for ( i = 0; i < gnNofOutDir; i++ ) {   // loop through total number of output directory

                memset(datleaf, 0x00, sizeof(datleaf));
                memset(dest_dat, 0x00, sizeof(dest_dat));
                memset(cmd, 0x00, sizeof(cmd));
                if ( gszIniSubParItem[i][E_LEAF_DIRDAT_][0] != '\0' ) {
                    prepLeafDir(gszIniSubParItem[i][E_LEAF_DIRDAT_], snp_inf, datetime, datleaf);
writeLog(LOG_DB3, "one2oneCopy prepLeafDir Dat '%s' -> '%s'", gszIniSubParItem[i][E_LEAF_DIRDAT_], datleaf);
                }
                
                sprintf(cmd, "mkdir -p %s/%s 2>/dev/null", gszIniSubParItem[i][E_ROOT_DIRDAT_], datleaf);
writeLog(LOG_DB3, "one2oneCopy cmd '%s'", cmd);
                system(cmd);
                
                sprintf(cmd, "chmod 755 %s/%s", gszIniSubParItem[i][E_ROOT_DIRDAT_], datleaf);
                system(cmd);
                
                sprintf(cmd, "cp -p %s %s/%s 2>/dev/null", ori_dat, gszIniSubParItem[i][E_ROOT_DIRDAT_], datleaf);
writeLog(LOG_DB3, "one2oneCopy cmd '%s'", cmd);

                sprintf(dest_dat, "%s/%s/%s", gszIniSubParItem[i][E_ROOT_DIRDAT_], datleaf, snp_inf[E_DAT_FILE]);
writeLog(LOG_DB3, "one2oneCopy dest_dat %s", dest_dat);
                retry = 3;
                do {
                    memset(ocksum2, 0x00, sizeof(ocksum2));
            
                    system(cmd);
                    
                    getCksumStr(dest_dat, ocksum2, sizeof(ocksum2));
                    trimStr(ocksum2);

                    if ( strcmp(ocksum1, ocksum2) != 0 ) {
                        retry--;
                        sleep(1);
                    }
                    else {
                        chmod(dest_dat, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
                        break;
                    }
                } while ( retry > 0 );
                if ( retry <= 0 ) {
                    writeLog(LOG_ERR, "copy file %s failed after a few tries", snp_inf[E_DAT_FILE]);
                    continue;
                }
                
                if ( gszIniSubParItem[i][E_CREATE_SYN_][0] == 'Y' && strcmp(gszIniParInput[E_SRC_TYPE], _SELF_) != 0 ) {
                    
                    memset(synleaf, 0x00, sizeof(synleaf));
                    memset(dest_syn, 0x00, sizeof(dest_syn));
                    
                    if ( gszIniSubParItem[i][E_LEAF_DIRSYN_][0] != '\0' ) {
                        prepLeafDir(gszIniSubParItem[i][E_LEAF_DIRSYN_], snp_inf, datetime, synleaf);
writeLog(LOG_DB3, "one2oneCopy prepLeafDir Syn '%s' -> '%s'", gszIniSubParItem[i][E_LEAF_DIRSYN_], synleaf);
                    }

                    sprintf(cmd, "mkdir -p %s/%s 2>/dev/null", gszIniSubParItem[i][E_ROOT_DIRSYN_], synleaf);
writeLog(LOG_DB3, "one2oneCopy cmd '%s'", cmd);
                    system(cmd);
                    
                    sprintf(cmd, "chmod 755 %s/%s", gszIniSubParItem[i][E_ROOT_DIRSYN_], synleaf);
                    system(cmd);
                    
                    sprintf(cmd, "cp -p %s %s/%s 2>/dev/null", ori_syn, gszIniSubParItem[i][E_ROOT_DIRSYN_], synleaf);
                    sprintf(dest_syn, "%s/%s/%s", gszIniSubParItem[i][E_ROOT_DIRSYN_], synleaf, snp_inf[E_SYN_FILE]);
                    chmod(dest_syn, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
                }
            }
            // keep state of which file has been processed
            // sort all state files (<APP_NAME>_<PROC_TYPE>_<YYYYMMDD>.proclist) to tmp file
            // state files format is <LEAF>|<SYNC_FILE>
            logState(snp_inf[E_LEAF_SYN], snp_inf[E_SYN_FILE]);

            // do backup and keep state here
            doBackup(snp_inf);

            writeLog(LOG_INF, "file %s copied (mtime=%s)", snp_inf[E_DAT_FILE], snp_inf[E_YMD14]);
            gnInpFileCntBat++;
            gnInpFileCntDay++;
        }
        gnOutFileCntBat = gnInpFileCntBat;
        writeLog(LOG_INF, "total processed files for this round in=%d out=%d", gnInpFileCntBat, gnOutFileCntBat);
        gtLastProcTimeT = time(NULL);   // reset last process time;
    }
    
    return SUCCESS;

}

void prepLeafDir(const char *orig, char snp[][SIZE_ITEM_L], const char *yyyymmdd, char *out)
{
    char *tmpout = NULL;
    char mmdd[SIZE_DATE_ONLY+1];
    char now_mmdd[SIZE_DATE_ONLY+1];
    char newsrc[SIZE_BUFF];

    memset(mmdd, 0x00, sizeof(mmdd));
    memset(now_mmdd, 0x00, sizeof(now_mmdd));
    memset(newsrc, 0x00, sizeof(newsrc));

    strncpy(mmdd, snp[E_YMD8]+4, 4);
    strncpy(now_mmdd, yyyymmdd+4, 4);

    strcpy(newsrc, orig);
    tmpout = strReplaceAll(newsrc, _SYN_NET_ID_COL_, snp[E_NEID]);
        strcpy(newsrc, tmpout);
        free(tmpout);
    tmpout = strReplaceAll(newsrc, _SYN_NET_TYPE_COL_, snp[E_NETTYPE]);
        strcpy(newsrc, tmpout);
        free(tmpout);
    tmpout = strReplaceAll(newsrc, _NOW_MMDD_, now_mmdd);
        strcpy(newsrc, tmpout);
        free(tmpout);
    tmpout = strReplaceAll(newsrc, _NOW_YYYYMMDD_, yyyymmdd);
        strcpy(newsrc, tmpout);
        free(tmpout);
    tmpout = strReplaceAll(newsrc, _SYN_MMDD_, mmdd);
        strcpy(newsrc, tmpout);
        free(tmpout);
    tmpout = strReplaceAll(newsrc, _SYN_YYYYMMDD_, snp[E_YMD8]);
        strcpy(newsrc, tmpout);
        free(tmpout);
    strcpy(out, newsrc);

}

void doBackup(char snp[][SIZE_ITEM_L])
{
    char fdata[SIZE_ITEM_L];
    char fsync[SIZE_ITEM_L];
    char datetime[SIZE_DATE_ONLY+1];
    char synleaf[SIZE_ITEM_L];
    char datleaf[SIZE_ITEM_L];
    char cmd[SIZE_BUFF];
    char ocksum1[SIZE_ITEM_S];
    char ocksum2[SIZE_ITEM_S];
    char ofdata[SIZE_ITEM_L];
    int retry = 3;

    memset(fdata, 0x00, sizeof(fdata));
    memset(fsync, 0x00, sizeof(fsync));
    memset(datetime, 0x00, sizeof(datetime));
    memset(synleaf, 0x00, sizeof(synleaf));
    memset(datleaf, 0x00, sizeof(datleaf));
    memset(cmd, 0x00, sizeof(cmd));

    sprintf(fdata, "%s/%s/%s", gszIniParInput[E_ROOT_DIRDAT], snp[E_LEAF_DAT], snp[E_DAT_FILE]);
    sprintf(fsync, "%s/%s/%s", gszIniParInput[E_ROOT_DIRSYN], snp[E_LEAF_SYN], snp[E_SYN_FILE]);
writeLog(LOG_DB3, "doBackup fdata(%s) fsync(%s)", fdata, fsync);
    strcpy(datetime, getSysDTM(DTM_DATE_ONLY));

    if ( gszIniParBackup[E_BACKUP_DAT][0] == 'Y' ) {
        if ( gszIniParBackup[E_BACKUP_SUBDAT][0] != '\0' ) {
            prepLeafDir(gszIniParBackup[E_BACKUP_SUBDAT], snp, datetime, datleaf);
        }
        sprintf(cmd, "mkdir -p %s/%s 2>/dev/null", gszIniParBackup[E_BACKUP_DIRDAT], datleaf);
writeLog(LOG_DB3, "doBackup cmd '%s'", cmd);
        system(cmd);
        
        sprintf(cmd, "chmod 755 %s/%s", gszIniParBackup[E_BACKUP_DIRDAT], datleaf);
        system(cmd);
        
        memset(ocksum1, 0x00, sizeof(ocksum1));
        memset(ofdata, 0x00, sizeof(ofdata));
        
        getCksumStr(fdata, ocksum1, sizeof(ocksum1));
        trimStr(ocksum1);
        
        sprintf(cmd, "cp %s %s/%s", fdata, gszIniParBackup[E_BACKUP_DIRDAT], datleaf);
        sprintf(ofdata, "%s/%s/%s", gszIniParBackup[E_BACKUP_DIRDAT], datleaf, snp[E_DAT_FILE]);
        
        do {    // loop for retry backup

writeLog(LOG_DB3, "doBackup cmd '%s'", cmd);
            memset(ocksum2, 0x00, sizeof(ocksum2));
            
            system(cmd);
            
            getCksumStr(ofdata, ocksum2, sizeof(ocksum2));
            trimStr(ocksum2);
            
writeLog(LOG_DB3, "doBackup compare ckusm src/dst (%s/%s)", ocksum1, ocksum2);
            if ( strcmp(ocksum1, ocksum2) != 0 ) {
                retry--;
                sleep(1);
            }
            else {
                chmod(ofdata, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
                break;
            }
        } while ( retry > 0 );
        if ( retry <= 0 ) {
            writeLog(LOG_ERR, "file size %s copy to back up not ok", snp[E_DAT_FILE]);
        }
    }

    if ( gszIniParBackup[E_BACKUP_SYN][0] == 'Y' && strcmp(gszIniParInput[E_SRC_TYPE], _SELF_) != 0 ) {
        if ( gszIniParBackup[E_BACKUP_SUBSYN][0] != '\0' ) {
            prepLeafDir(gszIniParBackup[E_BACKUP_SUBSYN], snp, datetime, synleaf);
        }
        sprintf(cmd, "mkdir -p %s/%s 2>/dev/null", gszIniParBackup[E_BACKUP_DIRSYN], synleaf);
writeLog(LOG_DB3, "doBackup cmd '%s'", cmd);
        system(cmd);
        
        sprintf(cmd, "chmod 755 %s/%s", gszIniParBackup[E_BACKUP_DIRSYN], synleaf);
        system(cmd);
        
        sprintf(cmd, "cp %s %s/%s", fsync, gszIniParBackup[E_BACKUP_DIRSYN], synleaf);
writeLog(LOG_DB3, "doBackup cmd '%s'", cmd);
        system(cmd);
    }

    if ( gszIniParCommon[E_REMOVE_DAT][0] == 'Y' ) {
        sprintf(cmd, "rm -f %s", fdata);
writeLog(LOG_DB3, "doBackup cmd '%s'", cmd);
        system(cmd);
    }

    if ( gszIniParCommon[E_REMOVE_SYN][0] == 'Y' ) {
        sprintf(cmd, "rm -f %s", fsync);
writeLog(LOG_DB3, "doBackup cmd '%s'", cmd);
        system(cmd);
    }

}

int mapNetType(char *nettype, char *cdrfeedtype)
{

    FILE *ifp;
    char cmd[SIZE_ITEM_L];
    char *var, *val;
    int result = FAILED;

    if ( gszIniParOutput[E_NW_MAP_FILE][0] != '\0' && strcmp(gszIniParOutput[E_NW_MAP_FILE], "NA") != 0 ) {
        if ( (ifp = fopen(gszIniParOutput[E_NW_MAP_FILE], "r")) == NULL ) {
            writeLog(LOG_ERR, "cannot open %s (%s)", gszIniParOutput[E_NW_MAP_FILE], strerror(errno));
            return FAILED;
        }
    }
    else {
        return SUCCESS;
    }

    memset(cmd, 0x00, sizeof(cmd));
    while ( fgets(cmd, sizeof(cmd), ifp) ) {
        if ( *cmd == '#' )                  // skip comment line.
            continue;
        if ( strtok(cmd, "#\r\n") == NULL ) // skip inline comment.
            continue;
        var = strtok(cmd, " \t");           // token by space and tab
        if ( var == NULL )
            continue;
        val = strtok(NULL, " \t");          // token by space and tab
        if ( val == NULL )
            continue;

        trimStr(var);
        trimStr(val);

        if ( strcmp(var, nettype) == 0 ) {
            result = SUCCESS;
            strcpy(cdrfeedtype, val);
            break;
        }

    }
    fclose(ifp);
    return result;

}

int logState(const char *leaf_dir, const char *file_name)
{
    int result = 0;
    if ( gfpState == NULL ) {
        char fstate[SIZE_ITEM_L];
        memset(fstate, 0x00, sizeof(fstate));
        sprintf(fstate, "%s/%s_%s%s", gszIniParCommon[E_STATE_DIR], gszAppName, gszToday, STATE_SUFF);
        gfpState = fopen(fstate, "a");
    }
    result = fprintf(gfpState, "%s|%s\n", leaf_dir, file_name);
    fflush(gfpState);

    return result;
}

int logMergeList(const char *oper, const char *file_name)
{
    char merge_file_list[SIZE_ITEM_L];
    if ( gszIniParCommon[E_MERGE_LOG_DIR][0] != '\0' ) {
        if ( gfpMerge == NULL ) {
            sprintf(merge_file_list, "%s/%s_%s%s", gszIniParCommon[E_MERGE_LOG_DIR], gszAppName, gszToday, MERGE_SUFF);
            if ( (gfpMerge = fopen(merge_file_list, "a")) == NULL ) {
                writeLog(LOG_ERR, "cannot open merge list %s for writing (%s), process continue", merge_file_list, strerror(errno));
                return FAILED;
            }
        }
    }
    if ( gfpMerge != NULL ) {
        int result = fprintf(gfpMerge, "%-8s %s\n", oper, file_name);
        fflush(gfpMerge);
        return result;
    }
    return SUCCESS;
}

void clearOldState()
{
    struct tm *ptm;
    time_t lTime;
    char tmp[SIZE_ITEM_L];
    char szOldestFile[SIZE_ITEM_S];
    char szOldestDate[SIZE_DATE_TIME_FULL+1];
    DIR *p_dir;
    struct dirent *p_dirent;
    int len1 = 0, len2 = 0;

    /* get oldest date to keep */
    time(&lTime);
    ptm = localtime( &lTime);
//printf("ptm->tm_mday = %d\n", ptm->tm_mday);
    ptm->tm_mday = ptm->tm_mday - atoi(gszIniParCommon[E_KEEP_STATE_DAY]);
//printf("ptm->tm_mday(after) = %d, keepState = %d\n", ptm->tm_mday, atoi(gszIniParCommon[E_KEEP_STATE_DAY]));
    lTime = mktime(ptm);
    ptm = localtime(&lTime);
    strftime(szOldestDate, sizeof(szOldestDate)-1, "%Y%m%d", ptm);
//printf("szOldestDate = %s\n", szOldestDate);

	writeLog(LOG_INF, "purge state file up to %s (keep %s days)", szOldestDate, gszIniParCommon[E_KEEP_STATE_DAY]);
    sprintf(szOldestFile, "%s%s", szOldestDate, STATE_SUFF);     // YYYYMMDD.proclist
    len1 = strlen(szOldestFile);
    if ( (p_dir = opendir(gszIniParCommon[E_STATE_DIR])) != NULL ) {
        while ( (p_dirent = readdir(p_dir)) != NULL ) {
            // state file name: <APP_NAME>_<PROC_TYPE>_YYYYMMDD.proclist
            if ( strcmp(p_dirent->d_name, ".") == 0 || strcmp(p_dirent->d_name, "..") == 0 )
                continue;
            if ( strstr(p_dirent->d_name, STATE_SUFF) != NULL &&
                 strstr(p_dirent->d_name, gszAppName) != NULL ) {

                len2 = strlen(p_dirent->d_name);
                // compare only last term of YYYYMMDD.proclist
                if ( strcmp(szOldestFile, (p_dirent->d_name + (len2-len1))) > 0 ) {
                    sprintf(tmp, "rm -f %s/%s 2>/dev/null", gszIniParCommon[E_STATE_DIR], p_dirent->d_name);
                    writeLog(LOG_INF, "remove state file: %s", p_dirent->d_name);
                    system(tmp);
                }
            }
        }
        closedir(p_dir);
    }
}

int readConfig(int argc, char *argv[])
{
    char appPath[SIZE_ITEM_L];
    char var[SIZE_ITEM_T];
    int key, vlen, i, endloop;

    memset(appPath, 0x00, sizeof(appPath));
    memset(gszIniFile, 0x00, sizeof(gszIniFile));
    memset(gszPrcType, 0x00, sizeof(gszPrcType));
    memset(gszAppName, 0x00, sizeof(gszAppName));
    memset(gszIniParInput, 0x00, sizeof(gszIniParInput));
    memset(gszIniParSynInf, 0x00, sizeof(gszIniParSynInf));
    memset(gszIniParOutput, 0x00, sizeof(gszIniParOutput));
    memset(gszIniParBackup, 0x00, sizeof(gszIniParBackup));
    memset(gszIniParCommon, 0x00, sizeof(gszIniParCommon));
    memset(gszIniSubParItem, 0x00, sizeof(gszIniSubParItem));

    strcpy(appPath, argv[0]);
    char *p = strrchr(appPath, '/');
    *p = '\0';

    for ( i = 1; i < argc; i++ ) {
        if ( strncmp(argv[i], "-t", 2) == 0 ) {
            if ( strcmp(argv[i]+2, "first") == 0 ) {        // build snap all available existing sync files
                gnCmdArg = E_FIRST;
            }
            else if ( strcmp(argv[i]+2, "new") == 0 ) {     // build snap only brand new sync files
                gnCmdArg = E_NEW;
            }
            else if ( strcmp(argv[i]+2, "valid") == 0 ) {   // build snap only brand new with already verified sync files
                gnCmdArg = E_VALID;
            }
            else if ( strcmp(argv[i]+2, "single") == 0 ) {  // process only single loop of processing (process all verified sync file)
                gnCmdArg = E_SINGLE;
            }
            else {
                gnCmdArg = E_NORMAL;
            }
        }
        else if ( strcmp(argv[i], "-n") == 0 ) {     // specified ini file
            strcpy(gszIniFile, argv[++i]);
        }
        else if ( strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0 ) {
            printUsage();
            return FAILED;
        }
        else if ( strcmp(argv[i], "-mkini") == 0 ) {
            makeIni();
            return FAILED;
        }
        else {
            strToLower(gszPrcType, argv[i]);    // specified process type
        }
    }

    if ( gszPrcType[0] != '\0' ) {
        if ( gszIniFile[0] == '\0' ) {
            sprintf(gszIniFile, "%s/%s_%s.ini", appPath, _APP_NAME_, gszPrcType);
        }
        sprintf(gszAppName, "%s_%s", _APP_NAME_, gszPrcType);
    }
    else {
        if ( gszIniFile[0] == '\0' ) {
            sprintf(gszIniFile, "%s/%s.ini", appPath, _APP_NAME_);
        }
        sprintf(gszAppName, "%s", _APP_NAME_);
    }

    if ( access(gszIniFile, F_OK|R_OK) != SUCCESS ) {
        fprintf(stderr, "unable to access ini file %s (%s)\n", gszIniFile, strerror(errno));
        return FAILED;
    }

    // Read config of INPUT Section
    for ( key = 0; key < E_NOF_PAR_INPUT; key++ ) {
        vlen = ini_gets(gszIniSecName[E_INPUT], gszIniStrInput[key], "NA", gszIniParInput[key], sizeof(gszIniParInput[key]), gszIniFile);
    }

    // Read config of SYNC_INFO Section
    for ( key = 0; key < E_NOF_PAR_SYN_INF; key++ ) {
        vlen = ini_gets(gszIniSecName[E_SYN_INF], gszIniStrSynInf[key], "NA", gszIniParSynInf[key], sizeof(gszIniParSynInf[key]), gszIniFile);
        //gnSynInfIndex[key] = atoi(gszIniParSynInf[key]);
    }

    // Read config of OUTPUT Section
    for ( key = 0; key < E_NOF_PAR_OUTPUT; key++ ) {
        vlen = ini_gets(gszIniSecName[E_OUTPUT], gszIniStrOutput[key], "NA", gszIniParOutput[key], sizeof(gszIniParOutput[key]), gszIniFile);
    }

    // Read config of BACKUP Section
    for ( key = 0; key < E_NOF_PAR_BACKUP; key++ ) {
        vlen = ini_gets(gszIniSecName[E_BACKUP], gszIniStrBackup[key], "NA", gszIniParBackup[key], sizeof(gszIniParBackup[key]), gszIniFile);
    }

    // Read config of COMMON Section
    for ( key = 0; key < E_NOF_PAR_COMMON; key++ ) {
        vlen = ini_gets(gszIniSecName[E_COMMON], gszIniStrCommon[key], "NA", gszIniParCommon[key], sizeof(gszIniParCommon[key]), gszIniFile);
    }

    // Read config of OUTPUT Sub Section
    gnNofOutDir = 0;
    endloop = FALSE;
    for ( i = 0; i < NOF_SUB_OUTDIR; i++ ) {                // loop through total number of output directory
        for ( key = 0; key < E_NOF_PAR_SUBOUT; key++ ) {    // loop through total parameter of subdirectory in ini
            memset(var, 0x00, sizeof(var));
            sprintf(var, "%s%d", gszIniStrSubOutput[key], i);
            vlen = ini_gets(gszIniSecName[E_OUTPUT], var, "NA", gszIniSubParItem[i][key], sizeof(gszIniSubParItem[i][key]), gszIniFile);
            // check to end loop if item number N is not config or config with empty string for the root directory
            if ( (key == E_ROOT_DIRDAT_ || key == E_ROOT_DIRSYN_) && (vlen == 0 || strcmp(gszIniSubParItem[i][key], "NA") == 0) ) {
                endloop = TRUE;
                break;
            }
        }
        if ( endloop == TRUE ) {
            break;
        }
        gnNofOutDir++;
    }

    return SUCCESS;

}

void logHeader()
{
    writeLog(LOG_INF, "---- Start %s %s (v%s) with following parameters ----", _APP_NAME_, gszPrcType, _APP_VERS_);
    // print out all ini file
    ini_browse(_ini_callback, NULL, gszIniFile);
}

void printUsage()
{
    fprintf(stderr, "\nusage: %s version %s\n", _APP_NAME_, _APP_VERS_);
    fprintf(stderr, "\tcopy or merge files to multi output directory\n\n");
    fprintf(stderr, "%s.exe <proc_type> [-t<test_option>] [-n <ini_file>] [-mkini]\n", _APP_NAME_);
    fprintf(stderr, "\tproc_type\tunique process name that represent data type\n");
    fprintf(stderr, "\ttest_option\tto test a specific function then stop\n");
    fprintf(stderr, "\t\tfirst\tlist all available sync files\n");
    fprintf(stderr, "\t\tnew\tlist all only new sync files\n");
    fprintf(stderr, "\t\tvalid\tlist all sync files that will be processed then stop\n");
    fprintf(stderr, "\t\tsingle\tperform single round of copy/merge (process all valid sync file)\n");
    fprintf(stderr, "\tini_file\tto specify ini file other than default ini\n");
    fprintf(stderr, "\t-mkini\t\tto create ini template\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\teg. to copy data type of GSM with single processing loop\n");
    fprintf(stderr, "\t./%s.exe gsm -tsingle\n", _APP_NAME_);
    fprintf(stderr, "\n");

}

int validateIni()
{

    int result = SUCCESS;

    // ----- Input section -----
    if ( strcmp(gszIniParInput[E_SRC_TYPE], _SELF_) != 0 &&
         strcmp(gszIniParInput[E_SRC_TYPE], _LEAF_IDEN_) != 0 &&
         strcmp(gszIniParInput[E_SRC_TYPE], _LEAF_SYNC_) != 0 ) {
        result = FAILED;
        fprintf(stderr, "%s must be one of %s, %s or %s (%s)\n", gszIniStrInput[E_SRC_TYPE], _SELF_, _LEAF_IDEN_, _LEAF_SYNC_, gszIniParInput[E_SRC_TYPE]);
    }
    if ( access(gszIniParInput[E_ROOT_DIRDAT], F_OK|R_OK) != SUCCESS ) {
        result = FAILED;
        fprintf(stderr, "unable to access %s %s (%s)\n", gszIniStrInput[E_ROOT_DIRDAT], gszIniParInput[E_ROOT_DIRDAT], strerror(errno));
    }
    if ( access(gszIniParInput[E_ROOT_DIRSYN], F_OK|R_OK) != SUCCESS ) {
        result = FAILED;
        fprintf(stderr, "unable to access %s %s (%s)\n", gszIniStrInput[E_ROOT_DIRSYN], gszIniParInput[E_ROOT_DIRSYN], strerror(errno));
    }
    //gszIniParInput[E_LEAF_DIRSYN] no need to validate
    //gszIniParInput[E_SYN_FN_PAT]  no need to validate
    //gszIniParInput[E_SYN_FN_EXT]  no need to validate
    //gszIniParInput[E_DAT_FN_EXT]  no need to validate
    if ( strcmp(gszIniParInput[E_SRC_TYPE], _SELF_) != 0 ) {
        if ( strcmp(gszIniParInput[E_DAT_FN_FROM], _SYNCNAME_) != 0 &&
             strcmp(gszIniParInput[E_DAT_FN_FROM], _SYN_DATANAME_COL_ ) != 0 ) {
            result = FAILED;
            fprintf(stderr, "%s must be %s or %s (%s)\n", gszIniStrInput[E_DAT_FN_FROM], _SYNCNAME_, _SYN_DATANAME_COL_, gszIniParInput[E_DAT_FN_FROM]);
        }
    }
    if ( strcmp(gszIniParInput[E_SRC_TYPE], _LEAF_SYNC_) != 0 ) {
        if ( strcmp(gszIniParInput[E_DAT_SUBDIR], _SYN_NET_ID_COL_) != 0 &&
             strcmp(gszIniParInput[E_DAT_SUBDIR], _SYN_NET_TYPE_COL_) != 0 ) {
            result = FAILED;
            fprintf(stderr, "%s must be %s or %s (%s)\n", gszIniStrInput[E_DAT_SUBDIR], _SYN_NET_ID_COL_, _SYN_NET_TYPE_COL_, gszIniParInput[E_DAT_SUBDIR]);
        }
    }

    // ----- Sync Info section -----
    if ( strcmp(gszIniParSynInf[E_SYN_INFFROM], _CONTENT_) != 0 &&
         strcmp(gszIniParSynInf[E_SYN_INFFROM], _SYNCNAME_) != 0 ) {
        result = FAILED;
        fprintf(stderr, "%s must be %s or %s (%s)\n", gszIniStrSynInf[E_SYN_INFFROM], _CONTENT_, _SYNCNAME_, gszIniParSynInf[E_SYN_INFFROM]);
    }
    //gszIniParSynInf[E_SYN_STABLE_SEC] no need to validate
    if ( strcmp(gszIniParSynInf[E_SYN_INFFROM], _SYNCNAME_) == 0 ) {
        if ( gszIniParSynInf[E_SYN_COL_DELI][0] == '|' ) {
            result = FAILED;
            fprintf(stderr, "%s cannot be %c for %s = %s\n", gszIniStrSynInf[E_SYN_COL_DELI], gszIniParSynInf[E_SYN_COL_DELI][0], gszIniStrSynInf[E_SYN_INFFROM], _SYNCNAME_);
        }
    }
    if ( atoi(gszIniParSynInf[E_SYN_NET_ID_COL]) < 0 ) {
        result = FAILED;
        fprintf(stderr, "%s must be started from 1 (0 - whole string) (%s)\n", gszIniStrSynInf[E_SYN_NET_ID_COL], gszIniParSynInf[E_SYN_NET_ID_COL]);
    }
    if ( atoi(gszIniParSynInf[E_SYN_NET_TYPE_COL]) < 0 ) {
        result = FAILED;
        fprintf(stderr, "%s must be started from 1 (0 - whole string) (%s)\n", gszIniStrSynInf[E_SYN_NET_TYPE_COL], gszIniParSynInf[E_SYN_NET_TYPE_COL]);
    }
    if ( atoi(gszIniParSynInf[E_SYN_DATNAME_COL]) < 0 ) {
        result = FAILED;
        fprintf(stderr, "%s must be started from 1 (0 - whole string) (%s)\n", gszIniStrSynInf[E_SYN_DATNAME_COL], gszIniParSynInf[E_SYN_DATNAME_COL]);
    }
    if ( atoi(gszIniParSynInf[E_SYN_DATSIZE_COL]) < 0 ) {
        result = FAILED;
        fprintf(stderr, "%s must be started from 1 (0 - whole string) (%s)\n", gszIniStrSynInf[E_SYN_DATSIZE_COL], gszIniParSynInf[E_SYN_DATSIZE_COL]);
    }

    // ----- Output section -----
    //gszIniParOutput[E_COPY_MODE]          no need to validate
    //gszIniParOutput[E_MRG_OUTPUT]         no need to validate
    //gszIniParOutput[E_MRG_MAX_SIZE_MB]    no need to validate
    if ( gszIniParOutput[E_NW_MAP_FILE][0] != '\0' && strcmp(gszIniParOutput[E_NW_MAP_FILE], "NA") != 0 ) {
        if ( access(gszIniParOutput[E_NW_MAP_FILE], F_OK|R_OK) != SUCCESS ) {
            result = FAILED;
            fprintf(stderr, "unable to access %s %s (%s)\n", gszIniStrOutput[E_NW_MAP_FILE], gszIniParOutput[E_NW_MAP_FILE], strerror(errno));
        }
    }

    if ( gszIniParOutput[E_DECODER_PRG][0] != '\0' && strcmp(gszIniParOutput[E_DECODER_PRG], "NA") != 0 ) {
        if ( access(gszIniParOutput[E_DECODER_PRG], F_OK|X_OK) != SUCCESS ) {
            result = FAILED;
            fprintf(stderr, "unable to access %s %s (%s)\n", gszIniStrOutput[E_DECODER_PRG], gszIniParOutput[E_DECODER_PRG], strerror(errno));
        }
    }

    // ----- Sub output -----
    int i;
    for ( i=0; i<gnNofOutDir; i++ ) {
        if ( access(gszIniSubParItem[i][E_ROOT_DIRDAT_], F_OK) != SUCCESS ) {
            result = FAILED;
            fprintf(stderr, "unable to access %s%d %s (%s)\n", gszIniStrSubOutput[E_ROOT_DIRDAT_], i, gszIniSubParItem[i][E_ROOT_DIRDAT_], strerror(errno));
        }
        if ( access(gszIniSubParItem[i][E_ROOT_DIRSYN_], F_OK) != SUCCESS ) {
            result = FAILED;
            fprintf(stderr, "unable to access %s%d %s (%s)\n", gszIniStrSubOutput[E_ROOT_DIRSYN_], i, gszIniSubParItem[i][E_ROOT_DIRSYN_], strerror(errno));
        }
        //gszIniSubParItem[i][E_LEAF_DIRDAT_]   no need to validate
        //gszIniSubParItem[i][E_LEAF_DIRSYN_]   no need to validate
        if ( gszIniSubParItem[i][E_CREATE_SYN_][0] == 'Y' || gszIniSubParItem[i][E_CREATE_SYN_][0] == 'y' ) {
            gszIniSubParItem[i][E_CREATE_SYN_][0] = 'Y';
        }
    }

    // ----- Backup section -----
    if ( gszIniParBackup[E_BACKUP_DAT][0] == 'Y' || gszIniParBackup[E_BACKUP_DAT][0] == 'y' ) {
        gszIniParBackup[E_BACKUP_DAT][0] = 'Y';
    }
    if ( gszIniParBackup[E_BACKUP_SYN][0] == 'Y' || gszIniParBackup[E_BACKUP_SYN][0] == 'y' ) {
        gszIniParBackup[E_BACKUP_SYN][0] = 'Y';
    }
    if ( access(gszIniParBackup[E_BACKUP_DIRDAT], F_OK) != SUCCESS ) {
        result = FAILED;
        fprintf(stderr, "unable to access %s %s (%s)\n", gszIniStrBackup[E_BACKUP_DIRDAT], gszIniParBackup[E_BACKUP_DIRDAT], strerror(errno));
    }
    if ( access(gszIniParBackup[E_BACKUP_DIRSYN], F_OK) != SUCCESS ) {
        result = FAILED;
        fprintf(stderr, "unable to access %s %s (%s)\n", gszIniStrBackup[E_BACKUP_DIRSYN], gszIniParBackup[E_BACKUP_DIRSYN], strerror(errno));
    }
    //gszIniParBackup[E_BACKUP_SUBDAT]  no need to validate
    //gszIniParBackup[E_BACKUP_SUBSYN]  no need to validate

    // ----- Common section -----
    if ( access(gszIniParCommon[E_TMP_DIR], F_OK) != SUCCESS ) {
        result = FAILED;
        fprintf(stderr, "unable to access %s %s (%s)\n", gszIniStrCommon[E_TMP_DIR], gszIniParCommon[E_TMP_DIR], strerror(errno));
    }
    if ( access(gszIniParCommon[E_LOG_DIR], F_OK) != SUCCESS ) {
        result = FAILED;
        fprintf(stderr, "unable to access %s %s (%s)\n", gszIniStrCommon[E_LOG_DIR], gszIniParCommon[E_LOG_DIR], strerror(errno));
    }
    //gszIniParCommon[E_LOG_LEVEL]  no need to validate
    if ( access(gszIniParCommon[E_STATE_DIR], F_OK) != SUCCESS ) {
        result = FAILED;
        fprintf(stderr, "unable to access %s %s (%s)\n", gszIniStrCommon[E_STATE_DIR], gszIniParCommon[E_STATE_DIR], strerror(errno));
    }
    if ( atoi(gszIniParCommon[E_KEEP_STATE_DAY]) <= 0 ) {
        result = FAILED;
        fprintf(stderr, "%s must be > 0 (%s)\n", gszIniStrCommon[E_KEEP_STATE_DAY], gszIniParCommon[E_KEEP_STATE_DAY]);
    }
    if ( gszIniParCommon[E_REMOVE_DAT][0] == 'Y' || gszIniParCommon[E_REMOVE_DAT][0] == 'y' ) {
        gszIniParCommon[E_REMOVE_DAT][0] = 'Y';
    }
    if ( gszIniParCommon[E_REMOVE_SYN][0] == 'Y' || gszIniParCommon[E_REMOVE_SYN][0] == 'y' ) {
        gszIniParCommon[E_REMOVE_SYN][0] = 'Y';
    }
    if ( atoi(gszIniParCommon[E_SLEEP_SEC]) <= 0 ) {
        result = FAILED;
        fprintf(stderr, "%s must be > 0 (%s)\n", gszIniStrCommon[E_SLEEP_SEC], gszIniParCommon[E_SLEEP_SEC]);
    }
    if ( atoi(gszIniParCommon[E_SKIP_OLD_FILE]) <= 0 ) {
        result = FAILED;
        fprintf(stderr, "%s must be > 0 (%s)\n", gszIniStrCommon[E_SKIP_OLD_FILE], gszIniParCommon[E_SKIP_OLD_FILE]);
    }
    if ( atoi(gszIniParCommon[E_NO_SYN_ALERT_HOUR]) <= 0 ) {
        result = FAILED;
        fprintf(stderr, "%s must be > 0 (%s)\n", gszIniStrCommon[E_NO_SYN_ALERT_HOUR], gszIniParCommon[E_NO_SYN_ALERT_HOUR]);
    }
    if ( gszIniParCommon[E_ALERT_LOG_DIR][0] != '\0' ) {
        if ( access(gszIniParCommon[E_ALERT_LOG_DIR], F_OK) != SUCCESS ) {
            result = FAILED;
            fprintf(stderr, "unable to access %s %s (%s)\n", gszIniStrCommon[E_ALERT_LOG_DIR], gszIniParCommon[E_ALERT_LOG_DIR], strerror(errno));
        }
    }

    return result;

}

int _ini_callback(const char *section, const char *key, const char *value, void *userdata)
{
    writeLog(LOG_INF, "[%s]\t%s = %s", section, key, value);
    return 1;
}

void makeIni()
{

    int key, i;
    char var[SIZE_ITEM_T];
    char cmd[SIZE_ITEM_S];
    char tmp_ini[SIZE_ITEM_S];
    char tmp_itm[SIZE_ITEM_S];

    sprintf(tmp_ini, "./%s_XXXXXX", _APP_NAME_);
    mkstemp(tmp_ini);
    strcpy(tmp_itm, "<place_holder>");

    // Write config of INPUT Section
    for ( key = 0; key < E_NOF_PAR_INPUT; key++ ) {
        i = ini_puts(gszIniSecName[E_INPUT], gszIniStrInput[key], tmp_itm, tmp_ini);
    }

    // Write config of SYNC_INFO Section
    for ( key = 0; key < E_NOF_PAR_SYN_INF; key++ ) {
        ini_puts(gszIniSecName[E_SYN_INF], gszIniStrSynInf[key], tmp_itm, tmp_ini);
    }

    // Write config of OUTPUT Section
    for ( key = 0; key < E_NOF_PAR_OUTPUT; key++ ) {
        ini_puts(gszIniSecName[E_OUTPUT], gszIniStrOutput[key], tmp_itm, tmp_ini);
    }

    // Write config of OUTPUT Sub Section
    for ( i = 0; i < NOF_SUB_OUTDIR; i++ ) {                // loop through total number of output directory
        for ( key = 0; key < E_NOF_PAR_SUBOUT; key++ ) {    // loop through total parameter of subdirectory in ini
            memset(var, 0x00, sizeof(var));
            sprintf(var, "%s%d", gszIniStrSubOutput[key], i);
            ini_puts(gszIniSecName[E_OUTPUT], var, tmp_itm, tmp_ini);
        }
    }

    // Write config of BACKUP Section
    for ( key = 0; key < E_NOF_PAR_BACKUP; key++ ) {
        ini_puts(gszIniSecName[E_BACKUP], gszIniStrBackup[key], tmp_itm, tmp_ini);
    }

    // Write config of COMMON Section
    for ( key = 0; key < E_NOF_PAR_COMMON; key++ ) {
        ini_puts(gszIniSecName[E_COMMON], gszIniStrCommon[key], tmp_itm, tmp_ini);
    }

    sprintf(cmd, "mv %s %s.ini", tmp_ini, tmp_ini);
    system(cmd);
    fprintf(stderr, "ini template file is %s.ini\n", tmp_ini);

}

void getStrToken(char item[][SIZE_ITEM_L], int item_size, char *str, char *sep)
{
    char *token, *string, *tofree;
    int i=0;

    tofree = string = strdup(str); i = 0;
    while ( (token = strsep(&string, sep) ) != NULL )
        if ( i < item_size )
            strcpy(item[i++], token);

    free(tofree);
}

void getItemFromStr(const char *str, char *fno, const char *sep, char *out)
{
    if ( *sep == '\0' || strcmp(sep, _FIXED_) == 0 ) { // fixed length items
        int beg, len;
        char sbeg[8], slen[8];
        memset(sbeg, 0x00, sizeof(sbeg));
        memset(slen, 0x00, sizeof(slen));
        // config must be in format of "start_position,length_to_get" -> "1,10" -> get 10 characters starting from first string
        getTokenItem(fno, 1, ',', sbeg);
        getTokenItem(fno, 2, ',', slen);
        beg = ( atoi(sbeg) <= 0 ? 1 : atoi(sbeg) ) - 1;
        len = ( atoi(slen) <= 0 ? strlen(str) : atoi(slen) );
        strncpy(out, str+beg, len);
    }
    else {  // delimiter separated items
        if ( atoi(fno) == 0 ) {  // copy whole str to out
            strcpy(out, str);
        }
        else {
            getTokenItem(str, atoi(fno), *sep, out);
        }
    }
}

void chkAlertNoSync()
{
    int alert_sec = atoi(gszIniParCommon[E_NO_SYN_ALERT_HOUR]) * 60 * 60;
    time_t systime = time(NULL);

    if ( alert_sec > 0 ) {
writeLog(LOG_DB2, "sys(%ld) 1st(%ld) new(%ld) val(%ld) alrt_sec(%d)", systime, gtTimeCap1stSyn, gtTimeCapNewSyn, gtTimeCapValSyn, alert_sec);
        if ( ( (systime - gtTimeCap1stSyn) > alert_sec && gtTimeCap1stSyn > 0 ) ||
             ( (systime - gtTimeCapNewSyn) > alert_sec && gtTimeCapNewSyn > 0 ) ||
             ( (systime - gtTimeCapValSyn) > alert_sec && gtTimeCapValSyn > 0 ) ) {

            FILE *f = NULL;
            memset(gszAlertFname1, 0x00, sizeof(gszAlertFname1));
            memset(gszAlertFname2, 0x00, sizeof(gszAlertFname2));
            
            sprintf(gszAlertFname1, "%s/%s_%s%s", gszIniParCommon[E_ALERT_LOG_DIR], gszAppName, gszToday, ALERT_SUFF);
            f = fopen(gszAlertFname1, "a");
            fprintf(f, "%s (%s hour) %s|%s\n", gszAppName, gszIniParCommon[E_NO_SYN_ALERT_HOUR], getDateTimeT(&gtLastProcTimeT, DTM_DATE_TIME_FULL), getSysDTM(DTM_DATE_TIME_FULL));
            fclose(f);

            sprintf(gszAlertFname2, "%s/%s_%s_%.2s%s", gszIniParCommon[E_ALERT_LOG_DIR], gszAppName, gszToday, getSysDTM(DTM_TIME_FORM), ALERT_SUFF);
            f = fopen(gszAlertFname2, "a");
            fprintf(f, "%s\n", gszAlertStr2);
            fclose(f);
            
            writeLog(LOG_INF, "no new file for %s hour, alert file created", gszIniParCommon[E_NO_SYN_ALERT_HOUR]);

            gtTimeCap1stSyn = 0L;
            gtTimeCapNewSyn = 0L;
            gtTimeCapValSyn = 0L;
        }
    }
    else {
        gtTimeCap1stSyn = 0L;
        gtTimeCapNewSyn = 0L;
        gtTimeCapValSyn = 0L;
    }
}

int extDecoder(const char *full_fname, char *full_decoded)
{
    char cmd[SIZE_BUFF], msg[SIZE_ITEM_S];
    struct stat st;

    strcpy(full_decoded, full_fname);
    if ( gszIniParOutput[E_DECODER_PRG][0] != '\0' && strcmp(gszIniParOutput[E_DECODER_PRG], "NA") != 0 ) {
        memset(cmd, 0x00, sizeof(cmd));
        memset(msg, 0x00, sizeof(msg));
        
        sprintf(full_decoded, "%s/%s.decoded", gszIniParCommon[E_TMP_DIR], basename((char *)full_fname));
        
        if ( strstr(gszIniParOutput[E_DECODER_PRG], "uncompress") != NULL ||
             strstr(gszIniParOutput[E_DECODER_PRG], "gunzip") != NULL ) {
            sprintf(cmd, "%s -c %s > %s 2>/dev/null", gszIniParOutput[E_DECODER_PRG], full_fname, full_decoded);
        }
        else {
            sprintf(cmd, "%s -i %s -o %s -s \"|\"", gszIniParOutput[E_DECODER_PRG], full_fname, full_decoded);
        }
        system(cmd);
        
        sprintf(msg, "%s", strerror(errno));
        lstat(full_decoded, &st);
        if ( access(full_decoded, F_OK|R_OK) != SUCCESS || st.st_size <= 0 ) {
            writeLog(LOG_SYS, "problem with unzip/decoder: %s", msg);
            return FAILED;
        }
    }
    return SUCCESS;
}


void chkToDelete(const char *fname_del, const char *fname_ori)
{
    if ( *fname_del != '\0' && strcmp(fname_del, fname_ori) != 0 ) {
        unlink(fname_del);
    }
}

void getCksumStr(const char *fname, char *cksum_str, size_t ck_size)
{
    char cmd[SIZE_BUFF];
    FILE *ifp = NULL;
    
    memset(cmd, 0x00, sizeof(cmd));
    sprintf(cmd, "cksum %s 2>/dev/null | gawk '{ print $1\"|\"$2 }'", fname);
    if ( (ifp = popen(cmd, "r")) == NULL ) {
        writeLog(LOG_ERR, "cannot get cksum of %s (%s)", fname, strerror(errno));
    }
    fgets(cksum_str, ck_size, ifp);
    pclose(ifp);
}

int chkStateAndConcat(const char *oFileName)
{
    int result = FAILED;
    DIR *p_dir;
    struct dirent *p_dirent;
    char cmd[SIZE_BUFF];
    memset(cmd, 0x00, sizeof(cmd));
    unlink(oFileName);

    if ( (p_dir = opendir(gszIniParCommon[E_STATE_DIR])) != NULL ) {
        while ( (p_dirent = readdir(p_dir)) != NULL ) {
            // state file name: <APP_NAME>_<PROC_TYPE>_YYYYMMDD.proclist
            if ( strcmp(p_dirent->d_name, ".") == 0 || strcmp(p_dirent->d_name, "..") == 0 )
                continue;

            if ( strstr(p_dirent->d_name, STATE_SUFF) != NULL &&
                 strstr(p_dirent->d_name, gszAppName) != NULL ) {
                char state_file[SIZE_ITEM_L];
                memset(state_file, 0x00, sizeof(state_file));
                sprintf(state_file, "%s/%s", gszIniParCommon[E_STATE_DIR], p_dirent->d_name);
                if ( access(state_file, F_OK|R_OK|W_OK) != SUCCESS ) {
                    writeLog(LOG_ERR, "unable to read/write file %s", state_file);
                    result = FAILED;
                    break;
                }
                else {
                    sprintf(cmd, "cat %s >> %s 2>/dev/null", state_file, oFileName);
                    system(cmd);
                    result = SUCCESS;
                }
            }
        }
        closedir(p_dir);
        return result;
    }
    else {
        return result;
    }
}

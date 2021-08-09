#ifndef __NFS_INI_DEF_H__
#define __NFS_INI_DEF_H__

#ifdef  __cplusplus
    extern "C" {
#endif

//#include "glb_str_def.h"

#define NOF_SUB_OUTDIR      10
#define _FIXED_             "FIX"
#define _SELF_              "SELF"
#define _LEAF_IDEN_         "LEAF_IDENTICAL"
#define _LEAF_SYNC_         "LEAF_FROM_SYNC"
#define _CONTENT_           "CONTENT"
#define _SYNCNAME_          "SYNCNAME"
#define _SYN_DATANAME_COL_  "SYN_DATANAME_COL"
#define _SYN_NET_ID_COL_    "SYN_NET_ID_COL"
#define _SYN_NET_TYPE_COL_  "SYN_NET_TYPE_COL"
#define _NOW_MMDD_          "NOW_MMDD"
#define _NOW_YYYYMMDD_      "NOW_YYYYMMDD"
#define _SYN_MMDD_          "SYN_MMDD"
#define _SYN_YYYYMMDD_      "SYN_YYYYMMDD"

typedef enum {
    E_INPUT = 0,
    E_SYN_INF,
    E_OUTPUT,
    E_BACKUP,
    E_COMMON,
    E_NOF_SECTION
} E_INI_SECTION;

typedef enum {
    // INPUT Section
    E_SRC_TYPE = 0,
    E_ROOT_DIRDAT,
    E_ROOT_DIRSYN,
    E_LEAF_DIRSYN,
    E_SYN_FN_PAT,
    E_SYN_FN_EXT,
    E_DAT_FN_EXT,
    E_DAT_FN_FROM,
    E_DAT_SUBDIR,
    E_NOF_PAR_INPUT
} E_INI_INPUT_SEC;

typedef enum {
    // SYNC_INFO Section
    E_SYN_INFFROM = 0,
    E_SYN_STABLE_SEC,
    E_SYN_COL_DELI,
    E_SYN_NET_ID_COL,
    E_SYN_NET_TYPE_COL,
    E_SYN_DATNAME_COL,
    E_SYN_DATSIZE_COL,
    E_NOF_PAR_SYN_INF
} E_INI_SYN_INF_SEC;

typedef enum {
    // OUTPUT Section
    E_COPY_MODE = 0,
    E_MRG_OUTPUT,
    E_MRG_MAX_SIZE_MB,
    E_FILE_PREFIX,
    E_NW_MAP_FILE,
    E_DECODER_PRG,
    E_NOF_PAR_OUTPUT
} E_INI_OUTPUT_SEC;

typedef enum {
    // BACKUP Section
    E_BACKUP_DAT = 0,
    E_BACKUP_SYN,
    E_BACKUP_DIRDAT,
    E_BACKUP_DIRSYN,
    E_BACKUP_SUBDAT,
    E_BACKUP_SUBSYN,
    E_NOF_PAR_BACKUP
} E_INI_BACKUP_SEC;

typedef enum {
    // COMMON Section
    E_TMP_DIR = 0,
    E_LOG_DIR,
    E_LOG_LEVEL,
    E_STATE_DIR,
    E_KEEP_STATE_DAY,
    E_REMOVE_DAT,
    E_REMOVE_SYN,
    E_SLEEP_SEC,
    E_SKIP_OLD_FILE,
    E_NO_SYN_ALERT_HOUR,
    E_ALERT_LOG_DIR,
    E_MERGE_LOG_DIR,
    E_NOF_PAR_COMMON
} E_INI_COMMON_SEC;

typedef enum {
    E_ROOT_DIRDAT_ = 0,
    E_ROOT_DIRSYN_,
    E_LEAF_DIRDAT_,
    E_LEAF_DIRSYN_,
    E_CREATE_SYN_,
    E_NOF_PAR_SUBOUT
} E_PARAM_SUBOUT;

#ifdef  __cplusplus
    }
#endif

#endif  // __NFS_INI_DEF_H__


#if !defined(__MGX_COMM_H__)
#define __MGX_COMM_H__

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define PKG_MAX_SIZE     30000  /* header + body */

/* receive status */
#define PKG_HDR_START       0
#define PKG_HDR_RECEIVING   1
#define PKG_BODY_START      2
#define PKG_BODY_RECEIVING  3

#define PKG_HDR_BUF_SIZE    20

#pragma pack(1) /* one byte align */

typedef struct {
    unsigned short  pkg_size;
    unsigned short  pkg_type;
    unsigned int    crc32;
} mgx_pkg_hdr_t, *pmgx_pkg_hdr_t;

#pragma pack()

#endif // __MGX_COMM_H__
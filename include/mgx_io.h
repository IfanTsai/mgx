#ifndef __MGX_IO_H__
#define __MGX_IO_H__

#define MGX_READ  0x01
#define MGX_WRITE 0x02

int mgx_poll(int fd, bool rd, bool wr, long timeout_ms);
int mgx_select(int fd, bool rd, bool wr, long timeout_ms);

#endif
#include "mgx_crc32.h"

Mgx_crc32 *Mgx_crc32::instance = nullptr;

Mgx_crc32::Mgx_crc32()
{
    init_crc32_table();
}

Mgx_crc32 *Mgx_crc32::get_instance()
{
    if (!instance) {
        instance = new Mgx_crc32();
    }
    return instance;
}

void Mgx_crc32::init_crc32_table()
{
    for (unsigned int i = 0; i < 256; i++) {
        unsigned int c = i;
        for (int j = 0; j < 8; j++) {
            if (c & 1)
                c = 0xedb88320L ^ (c >> 1);
            else
                c = c >> 1;
        }
        crc32_table[i] = c;
    }
}

unsigned int Mgx_crc32::get_crc32(unsigned char *buf, unsigned int size, unsigned int crc)
{
    for (unsigned int i = 0; i < size; i++)
        crc = crc32_table[(crc ^ buf[i]) & 0xff] ^ (crc >> 8);

    return crc ;
}
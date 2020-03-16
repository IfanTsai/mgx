#if !defined(__MGX_CRC32_H__)
#define __MGX_CRC32_H__

class Mgx_crc32
{
private:
    Mgx_crc32();

public:
    ~Mgx_crc32() {}

private:
    static Mgx_crc32 *instance;
    unsigned int crc32_table[256];

    void init_crc32_table();
public:
    static Mgx_crc32 *get_instance();

    unsigned int get_crc32(unsigned char *buf, unsigned int size, unsigned int crc=0xffffffff);
};

#endif // __MGX_CRC32_H__

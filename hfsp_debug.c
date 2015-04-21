#include <sys/param.h>
#include <sys/endian.h>

#include "hfsp_debug.h"

struct utf8_table {
    int cmask;
    int cval;
    int shift;
    long lmask;
    long lval;
};

static struct utf8_table utf8_table[] =
{
    {0x80,  0x00,   0*6,    0x7F,           0,         /* 1 byte sequence */},
    {0xE0,  0xC0,   1*6,    0x7FF,          0x80,      /* 2 byte sequence */},
    {0xF0,  0xE0,   2*6,    0xFFFF,         0x800,     /* 3 byte sequence */},
    {0xF8,  0xF0,   3*6,    0x1FFFFF,       0x10000,   /* 4 byte sequence */},
    {0xFC,  0xF8,   4*6,    0x3FFFFFF,      0x200000,  /* 5 byte sequence */},
    {0xFE,  0xFC,   5*6,    0x7FFFFFFF,     0x4000000, /* 6 byte sequence */},
    {0,                                                /* end of table    */}
};

int
hfsp_utf8_wctomb(char * sp, u_int16_t wc, int maxLen)
{
    long l;
    int c, nc;
    struct utf8_table *t;

    if (sp == 0)
        return 0;

    l = wc;
    nc = 0;
    for (t = utf8_table; t->cmask && maxLen; t++, maxLen--) 
    {
        nc++;
        if (l <= t->lmask) 
        {
            c = t->shift;
            *sp = t->cval | (l >> c);
            while (c > 0) 
            {
                c -= 6;
                sp++;
                *sp = 0x80 | ((l >> c) & 0x3F);
            }
            return nc;
        }
    }
    return -1;
}

int
hfsp_uni2asc(struct hfsp_unistr * ustrp, char * astrp, int len)
{
    int left, process, size;
    u_int16_t c;

    process = 0;
    left = len;
    while (process < ustrp->hu_len && left > 0)
    {
        c = be16toh(ustrp->hu_str[process]);
        if (!c || c > 0x7f)
        {
            size = hfsp_utf8_wctomb(astrp+process, c ? c : 0x2400, left);
            if (size != -1)
            {
                process +=size;
                left += size;
            }
        }
        else
        {
            astrp[process] = (char)c;
            left--;
            process++;
        }
    }

    if (process < ustrp->hu_len - 1)
        return EINVAL;

    return 0;
}

// XXX: To remove, only for debugging
void
udump(char * buff, int size)
{
    int i, left;
    left = size;
    do
    {
        for(i = 0; i < 8 && left > 0; i++, left--)
        {
            uprintf("%02x ", (u_int8_t)buff[size - left]);
        }
        uprintf("\n");
    } while (left > 0);
}

void
uprint_record_key(struct hfsp_record_key * rkp)
{
    char buf[250];

    bzero(buf, sizeof(buf));

    hfsp_uni2asc(&(rkp->hk_name), buf, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
    uprintf("Key length: %d\nKey parent cnid: %d\nKey name: \"%s\"\n", rkp->hk_len, rkp->hk_cnid, buf);
}

// XXX: To remove, only for debugging
void
uprint_record(struct hfsp_record * rp)
{
    struct hfsp_record_thread * rtp;
    char  buf[250];

    bzero(buf, sizeof(buf));

    uprintf("\n");
    uprint_record_key(&rp->hr_key);
    switch (rp->hr_type)
    {
        case HFSP_FILE_THREAD_RECORD:
        case HFSP_FOLDER_THREAD_RECORD:
            uprintf("Thread %s record\n", rp->hr_type == HFSP_FOLDER_THREAD_RECORD ? "folder" : "file");
            rtp = &rp->hr_thread;
            hfsp_uni2asc(&(rtp->hrt_name), buf, sizeof(buf));
            buf[sizeof(buf) - 1] = '\0';
            uprintf("Parent cnid: %d\nThread name %s \n", rtp->hrt_parentCnid, buf);
            break;

        case HFSP_FOLDER_RECORD:
            uprintf("Folder record\n");
            break;

        case HFSP_FILE_RECORD:
            uprintf("File record\n");
            break;

        default:
            uprintf("Unkmown record type\n");
    }
}



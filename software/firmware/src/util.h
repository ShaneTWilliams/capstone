
#define ABS(a)  ((a) < 0 ? -(a) : (a))
#define SIGN(a) ((a) < 0 ? -1 : 1)

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define SWAP16(val) ((((val) >> 8) & 0xff) | (((val)&0xff) << 8))
#define SWAP32(val)                                                             \
    ((((val) >> 24) & 0xff) | (((val) >> 8) & 0xff00) | (((val)&0xff00) << 8) | \
     (((val)&0xff) << 24))

// Hope we're targeting ARM...

#define htobe16(val) SWAP16(val)
#define htobe32(val) SWAP32(val)
#define htole16(val) (val)
#define htole32(val) (val)

#define be16toh(val) SWAP16(val)
#define be32toh(val) SWAP32(val)
#define le16toh(val) (val)
#define le32toh(val) (val)

#define MA_TO_A(val) ((val) / 1000.0f)
#define A_TO_MA(val) ((val)*1000.0f)
#define MV_TO_V(val) ((val) / 1000.0f)
#define V_TO_MV(val) ((val)*1000.0f)

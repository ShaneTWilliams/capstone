#include <stdio.h>

#include "lm75b.h"

int main(void) {
    uint16_t reg = 0x601f;
    float temp = LM75B_GET_TEMP_VALUE_FMT(reg);
    unsigned int temp_val = LM75B_GET_TEMP_VALUE(reg);
    printf("Hey\n");
    printf("%f\n", temp);
    printf("%d\n", temp_val);
    printf("%f\n",
        (double)((int32_t)((temp_val) << ((sizeof(int32_t) * 8) - (LM75B_TEMP_VALUE_SIZE)))) /
        (1 << ((sizeof(int32_t) * 8) - (LM75B_TEMP_VALUE_SIZE))) * (LM75B_TEMP_VALUE_LSB)
    );
}

gcc \
    local.c software/firmware/src/app/generated/lm75b.c \
    -o local \
    -Isoftware/firmware/src/app/generated \
    -Isoftware/firmware/src/app \
    -Isoftware/firmware/src

./local

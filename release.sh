clang main.c glad_gl.c -I inc -Ofast -lglfw -lm -o coin
i686-w64-mingw32-gcc main.c glad_gl.c -Ofast -I inc -Llib -lglfw3dll -lm -o coin.exe
strip --strip-unneeded coin
strip --strip-unneeded coin.exe
upx --lzma --best coin
upx --lzma --best coin.exe
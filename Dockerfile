FROM alpine:3.22 AS builder

LABEL authors="thiagorigonatti"

RUN apk add --no-cache cmake build-base musl-dev

WORKDIR /app

COPY CMakeLists.txt .
COPY .. .

RUN cmake -S . -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_FLAGS_RELEASE="-Ofast -march=znver2 -mtune=znver2 -pthread -static \
                               -finline-functions -funroll-loops -flto \
                               -falign-functions=32 -falign-loops=32 -falign-jumps=32 \
                               -fno-plt -fomit-frame-pointer -ffast-math -pipe" \
      -DCMAKE_EXE_LINKER_FLAGS="-static -flto -Wl,-O2,--as-needed,-z,now,--gc-sections,--strip-all" \
    && cmake --build build --config Release --parallel $(nproc) \
    && strip --strip-all build/rinha-2025


FROM scratch
COPY --from=builder /app/build/rinha-2025 /rinha-2025
ENTRYPOINT ["/rinha-2025"]

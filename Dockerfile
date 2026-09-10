# Multi-stage Dockerfile for PiFmRds-Enchanted (2026 Edition)
FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build -j$(nproc) && \
    ctest --test-dir build --output-on-failure

FROM debian:bookworm-slim

LABEL org.opencontainers.image.title="PiFmRds-Enchanted"
LABEL org.opencontainers.image.description="Modern FM/RDS Transmitter with MPX stereo, RT+, and SDR I/Q streaming"
LABEL org.opencontainers.image.source="https://github.com/deloskiytbackup/PiFmRds-Enchanted"
LABEL org.opencontainers.image.licenses="GPL-3.0"

WORKDIR /app
COPY --from=builder /app/build/src/pi_fm_rds /usr/local/bin/pi_fm_rds
COPY --from=builder /app/build/src/rds_wav /usr/local/bin/rds_wav
COPY --from=builder /app/tools/pifm-ctl.py /usr/local/bin/pifm-ctl
COPY --from=builder /app/src/sound.wav /app/sound.wav

RUN ln -s /usr/local/bin/pi_fm_rds /usr/local/bin/pifm-enchanted

ENTRYPOINT ["pi_fm_rds"]
CMD ["--help"]

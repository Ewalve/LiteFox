# сборка проекта LiteFox в контейнере Docker
FROM ubuntu:24.04 AS builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    openssl \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --config Release



# приложение LiteFox
FROM ubuntu:24.04

WORKDIR /app

COPY --from=builder /app/build/LiteFox . 
CMD ["./LiteFox"]
# сборка LiteFox C++ backend в контейнере Docker
FROM ubuntu:24.04 AS litefox_backend_builder
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


# сборка litefox react в контейнере Docker
FROM node:22-alpine AS litefox_frontend_builder
WORKDIR /app/frontend
COPY frontend/package*.json ./
RUN npm install
COPY frontend/ ./
RUN npm run build


# приложение LiteFox
FROM ubuntu:24.04

WORKDIR /app

COPY --from=litefox_backend_builder /app/build/LiteFox .
COPY --from=litefox_frontend_builder /app/frontend/dist ./frontend/dist
#COPY certs .
CMD ["./LiteFox"]
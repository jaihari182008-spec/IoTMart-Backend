
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libpqxx-dev \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build -j2

EXPOSE 10000

CMD ["sh", "-c", "./build/iotmart_server"]

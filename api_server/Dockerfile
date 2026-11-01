# api_server/Dockerfile

# --- Stage 1: Build Environment ---
FROM ubuntu:22.04 AS builder

# Install build dependencies (g++, cmake, asio for Crow, gtest, gmock)
RUN apt update && \
    apt install -y build-essential wget cmake pkg-config libasio-dev libpqxx-dev libgtest-dev

WORKDIR /usr/src/app

# Download the Crow single-header file
RUN wget https://github.com/CrowCpp/Crow/releases/download/v1.3.4/crow_all.h -O /usr/local/include/crow.h

# Build Google Test (since libgtest-dev only provides sources)
RUN cd /usr/src/googletest && \
    cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF && \
    cmake --build . --target install

# Copy your source code
COPY src ./src
COPY tests ./tests
COPY CMakeLists.txt .

# Compile the application
RUN cmake -B build -S .
RUN cmake --build build

# --- Stage 2: Minimal Runtime Environment ---
FROM ubuntu:22.04

# If you plan to connect to Postgres from C++, you'll need libpq at runtime
RUN apt-get update && apt-get install -y libpq5 libpqxx-6.4 && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the compiled binary from the builder stage
COPY --from=builder /usr/src/app/build/api_server ./api_server_bin
COPY --from=builder /usr/src/app/build/api_server_tests ./api_server_tests

EXPOSE 8080

# Run the binary
CMD ["./api_server_bin"]

# Test target for development
# To run tests: docker run api_server /app/api_server_tests

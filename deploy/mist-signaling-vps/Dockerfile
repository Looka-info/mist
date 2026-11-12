# Multi-stage Docker build for Mist Signaling Server
# Produces a minimal (~15MB) container with zero runtime dependencies.
FROM rust:1.80-alpine AS builder

RUN apk add --no-cache musl-dev

WORKDIR /app
COPY signaling/Cargo.toml signaling/Cargo.lock ./
COPY signaling/src ./src

RUN cargo build --release --locked

# Final minimal runner stage
FROM alpine:3.20

RUN apk add --no-cache ca-certificates tzdata

WORKDIR /app
COPY --from=builder /app/target/release/mist-signaling /app/mist-signaling

EXPOSE 8080

ENV RUST_LOG=info
ENTRYPOINT ["/app/mist-signaling", "--bind", "0.0.0.0:8080"]

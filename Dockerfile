# Multi-stage Docker build for Koyeb / Cloud deployment of Mist Signaling
FROM rust:alpine AS builder

RUN apk add --no-cache musl-dev

WORKDIR /app
COPY services/signaling/Cargo.toml ./
COPY services/signaling/src ./src

RUN cargo build --release

# Final minimal runner stage
FROM alpine:3.20

RUN apk add --no-cache ca-certificates tzdata

WORKDIR /app
COPY --from=builder /app/target/release/mist-signaling /app/mist-signaling

EXPOSE 8000
EXPOSE 8080

ENV RUST_LOG=info
CMD ["/app/mist-signaling"]

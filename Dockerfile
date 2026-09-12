# syntax=docker/dockerfile:1
# ---------------------------------------------------------------------------
# PawnHub — single-image deploy: build the React UI, compile the C++ engine for
# Linux, and serve both from one FastAPI process. Works on Render, Railway, Fly,
# or any Docker host. The engine is portable C++20 (std::popcount + multiply-based
# magic bitboards, no CPU-specific intrinsics), so it builds for any x86-64 host.
# ---------------------------------------------------------------------------

# 1) Build the frontend (Vite -> web/dist)
FROM node:20-slim AS web
WORKDIR /web
COPY web/package.json web/package-lock.json ./
RUN npm ci
COPY web/ ./
RUN npm run build

# 2) Compile the engine to a native Linux binary named "engine"
FROM gcc:14 AS engine
WORKDIR /src
COPY engine/src ./engine/src
RUN mkdir -p engine/build && \
    g++ -std=c++20 -O2 -march=x86-64-v2 -static \
        engine/src/*.cpp -o engine/build/engine && \
    ./engine/build/engine --selftest || true

# 3) Runtime: Python + FastAPI serving API and the built UI
FROM python:3.12-slim AS runtime
WORKDIR /app

COPY server/requirements.txt ./server/requirements.txt
RUN pip install --no-cache-dir -r server/requirements.txt

# App code, the trained net, the compiled engine, and the built UI
COPY server/ ./server/
COPY nets/ ./nets/
COPY --from=engine /src/engine/build/engine ./engine/build/engine
COPY --from=web /web/dist ./web/dist
RUN chmod +x ./engine/build/engine

ENV PORT=8000
EXPOSE 8000
# Hosts (Render/Railway) inject $PORT; fall back to 8000 locally.
CMD ["sh", "-c", "uvicorn server.app:app --host 0.0.0.0 --port ${PORT:-8000}"]

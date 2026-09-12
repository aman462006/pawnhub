# Deploying PawnHub

The whole app — the C++ engine, the FastAPI backend, and the React UI — ships as **one Docker
image**. The Dockerfile compiles the engine for Linux, builds the frontend, and serves the API and
the built site from a single process, so it runs on any Docker host.

## What's in the image

1. **UI build** (`node:20`) → `web/dist`
2. **Engine build** (`gcc:14`) → a native Linux `engine` binary (portable C++20, no CPU-specific
   intrinsics, compiled `-march=x86-64-v2` for broad cloud-CPU compatibility)
3. **Runtime** (`python:3.12-slim`) → FastAPI + `python-chess` serving `/api/*` and the static UI

The server auto-detects the engine binary name (`engine` on Linux, `engine.exe` on Windows) and binds
to the host-provided `$PORT`.

## Test it locally first

```bash
docker build -t pawnhub .
docker run --rm -p 8000:8000 pawnhub
# open http://localhost:8000
```

## Deploy to Render (recommended)

A `render.yaml` blueprint is included.

1. Push this repo to GitHub (already done).
2. In the [Render dashboard](https://dashboard.render.com): **New → Blueprint**, select the
   `pawnhub` repo, and confirm. Render reads `render.yaml`, builds the Dockerfile, and deploys.
3. Or without the blueprint: **New → Web Service → Docker**, pick the repo; Render finds the
   Dockerfile automatically. Health check path: `/api/bots`.

CLI alternative (needs a one-time interactive login you run yourself):

```bash
render login          # opens a browser
render blueprint launch
```

## Deploy to Railway

Railway auto-detects the Dockerfile — no extra config needed.

```bash
railway login         # interactive
railway init
railway up
```

Or in the Railway dashboard: **New Project → Deploy from GitHub repo → pawnhub**.

## Notes

- **Free tiers sleep.** On Render's free plan the service spins down when idle and takes a few
  seconds to wake on the next request. The first engine move after a cold start is slower.
- **Strength vs. resources.** "Aman" thinks for ~1 s per move; on a small free instance that's a
  little weaker than on a fast desktop. Bump the instance size (or lower Aman's movetime in
  `server/app.py`) if you want it snappier.
- **Training/Stockfish are not in the image.** Only the runtime (engine + net + web) is deployed;
  the training pipeline stays local.

# Car Part Connection — C Backend (Skeleton)

This is Phase 0: a minimal, dependency-free C HTTP server that exposes `GET /api/health` and logs structured JSON.

We’ll progressively add:
- Redis (jobs & sessions), PostgreSQL (users/vehicles), proper router, vendor scrapers, etc.

## Build

```bash
mkdir -p build && cd build
cmake ..
cmake --build . -j

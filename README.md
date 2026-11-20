# CoBo viewer

C++17 GRAW revision-5 decoder with nanobind, FastAPI, and Vue.

```sh
pnpm --dir frontend install
pnpm --dir frontend run build
uv sync --group dev
uv run cobo-viewer --workspace /path/to/data
```

Open http://127.0.0.1:8000. For frontend development use `pnpm --dir frontend dev`.

Optional `--config viewer.toml`:

```toml
workspace = "/path/to/data"
patterns = ["run_{run}/mm{cobo}", "mm_{cobo}/run_{run}"]
```

Patterns must be a list of paths relative to the workspace, using the literal
placeholders `{run}` and `{cobo}`. For data stored at
`/mnt/mm0/white/run_0000/*.graw`, use:

```toml
workspace = "/mnt"
patterns = ["mm{cobo}/white/run_{run}"]
```

Start with `uv run cobo-viewer --config config.toml` to apply the configuration.

Select a run, then use h/l for events and j/k for traces. Purple outlines identify FPN channels. Raw missing samples are gaps. Frame provenance is available beneath the waveform. Matching event IDs merge within a run.

Validation: `uv run pytest`, `pnpm --dir frontend typecheck`, and `pnpm --dir frontend build`.
Standalone native build: `cmake -S . -B build/native -DBUILD_BINDINGS=OFF` then `cmake --build build/native`.

Current implementation limitations: indexing is synchronous and memory-only; no background prefetch or persistent index yet. The server retains up to 32 native file readers. Binding arrays currently convert to Python lists, and the API sends whole decoded events. The plot uses SVG, without Plotly interaction. Performance benchmarks, real acquisition-file validation, and rendered browser verification are pending. This is an initial implementation, not completion of the full design.

## Code formatting

Format only project sources (the linked reference repositories are excluded):

```sh
uv run clang-format -i graw/include/reader.h graw/src/reader.cpp graw/bindings/module.cpp
uv run cmake-format -i CMakeLists.txt graw/CMakeLists.txt
uv run ruff check --select I --fix src/cobo_viewer tests
uv run ruff format src/cobo_viewer tests
pnpm --dir frontend format
```

C++ and CMake use tab indentation, following the reference C++ projects. Python uses four spaces; Vue and TypeScript use two spaces, double quotes, and semicolons, following estimator's frontend. Formatter configurations and dependency lockfiles make these conventions repeatable.

Check without rewriting using `clang-format --dry-run --Werror`, `cmake-format --check`, `ruff format --check`, and `pnpm --dir frontend format:check` with the same source paths. Run `uv run ruff check src/cobo_viewer tests` for Python lint checks.

## Native reader and frame headers

`graw::GrawReader` opens a binary file in its constructor and closes it through RAII. `Read()` returns the next frame, including a host-order `FrameHeader` and reconstructed traces, or `std::nullopt` at clean EOF. `Rewind()` restarts sequential reading; `Index()` scans headers without decoding payloads and rewinds afterward; `ReadAt(offset, ordinal)` supports browsing and leaves the reader positioned after that frame. A frame is one AsAd contribution; the Python service merges contributions sharing a hardware event ID.

`RawHeader`, `PartialData`, and `CompactData` describe the revision-5 binary layout with named fields and bitfields. Wire values are converted from big endian before data-word bitfields are used. Packed layout assertions and a runtime bitfield check currently require a compatible little-endian compiler/host. The reusable payload buffer belongs to the reader; returned frame data owns its samples. A reader must be externally serialized when shared between threads.

The frontend shows the selected trace's source-frame header, including event/time, format sizes, data source, CoBo/AsAd, status, read offset, per-AGET hit-pattern bytes, multiplicities, window output, and last-cell indices. Reserved header bytes are also retained and expandable.

## Dependency release policy

The frontend configures `minimumReleaseAge: 1440` in `frontend/pnpm-workspace.yaml`, requiring dependencies to be at least one day old. It overrides `source-map-js` to version `1.2.1` (published September 2024) to avoid the recent release rejected by the installation policy. Keep the lockfile when installing: `pnpm --dir frontend install --frozen-lockfile`.

import argparse
import re
import threading
import tomllib
from collections import OrderedDict
from contextlib import asynccontextmanager
from pathlib import Path

from fastapi import FastAPI, HTTPException
from fastapi.staticfiles import StaticFiles

from . import _bindings

HEADER_FIELDS = [
    "meta_type",
    "data_source",
    "revision",
    "cobo_id",
    "asad_id",
    "status",
    "frame_size",
    "item_count",
    "event_id",
    "window_out",
    "frame_type",
    "header_size",
    "item_size",
    "read_offset",
    "event_time",
    "hit_patterns",
    "multiplicities",
    "last_cells",
    "reserved",
]


def create_app(workspace, patterns=None):
    root = Path(workspace).resolve()
    if patterns is None:
        patterns = ["run_{run}/mm{cobo}", "mm_{cobo}/run_{run}"]
    if not isinstance(patterns, list) or not patterns:
        raise ValueError(
            'patterns must be a non-empty list, e.g. ["mm{cobo}/white/run_{run}"]'
        )
    for pattern in patterns:
        if (
            not isinstance(pattern, str)
            or Path(pattern).is_absolute()
            or "{run}" not in pattern
            or "{cobo}" not in pattern
        ):
            raise ValueError(
                "Each pattern must be a relative path containing {run} and {cobo}, "
                'e.g. "mm{cobo}/white/run_{run}"'
            )
    readers = OrderedDict()

    @asynccontextmanager
    async def lifespan(app):
        yield
        readers.clear()

    app = FastAPI(lifespan=lifespan)

    def reader(path):
        if path not in readers:
            readers[path] = _bindings.GrawReader(path)
            if len(readers) > 32:
                readers.popitem(last=False)
        readers.move_to_end(path)
        return readers[path]

    runs, indexes, cache = {}, {}, OrderedDict()
    lock = threading.RLock()

    def discover():
        found = {}
        for pattern in patterns:
            regex = (
                re.escape(pattern)
                .replace(r"\{run\}", "(?P<run>[^/]+)")
                .replace(r"\{cobo\}", "[^/]+")
            )
            glob = pattern.replace("{run}", "*").replace("{cobo}", "*")
            for directory in root.glob(glob):
                match = re.fullmatch(regex, directory.relative_to(root).as_posix())
                if match and directory.is_dir():
                    found.setdefault(match["run"], set()).update(
                        p.resolve()
                        for p in directory.rglob("*.graw")
                        if p.resolve().is_relative_to(root)
                    )
        runs.clear()
        runs.update(found)

    @app.get("/api/runs")
    def list_runs():
        with lock:
            discover()
            return sorted(runs)

    def index(run):
        if run not in runs:
            discover()
        if run not in runs:
            raise HTTPException(404, "Run not found")
        signature = tuple(
            (str(p), p.stat().st_size, p.stat().st_mtime_ns) for p in sorted(runs[run])
        )
        if run not in indexes or indexes[run][0] != signature:
            readers.clear()
            events = {}
            for path, *_ in signature:
                previous = -1
                for frame in reader(path).index():
                    if frame.event < previous:
                        raise ValueError("Event ID reset: " + path)
                    previous = frame.event
                    events.setdefault(frame.event, []).append(frame)
            indexes[run] = signature, events
            cache.clear()
        return indexes[run][1]

    @app.get("/api/runs/{run}/events")
    def events(run: str):
        with lock:
            try:
                return sorted(index(run))
            except (RuntimeError, ValueError) as e:
                raise HTTPException(422, str(e))

    @app.get("/api/runs/{run}/events/{event}")
    def event(run: str, event: int):
        with lock:
            try:
                frames = index(run).get(event)
                if frames is None:
                    raise HTTPException(404, "Event not found")
                key = (run, event)
                if key not in cache:
                    traces = []
                    for f in frames:
                        decoded = reader(f.path).read_at(f.offset, f.ordinal)
                        header = {
                            name: getattr(decoded.header, name)
                            for name in HEADER_FIELDS
                        }
                        for t in decoded.traces:
                            traces.append(
                                dict(
                                    header=header,
                                    cobo=f.cobo,
                                    asad=f.asad,
                                    aget=t.aget,
                                    channel=t.channel,
                                    path=str(Path(f.path).relative_to(root)),
                                    frame=f.ordinal,
                                    offset=f.offset,
                                    timestamp=f.time,
                                    samples=[
                                        v if p else None
                                        for v, p in zip(t.samples, t.present)
                                    ],
                                )
                            )
                    traces.sort(
                        key=lambda t: (
                            t["cobo"],
                            t["asad"],
                            t["aget"],
                            t["channel"],
                            t["path"],
                            t["frame"],
                        )
                    )
                    cache[key] = traces
                    while (
                        sum(len(v) * 20000 for v in cache.values()) > 256 * 1024 * 1024
                        and len(cache) > 1
                    ):
                        cache.popitem(last=False)
                cache.move_to_end(key)
                return cache[key]
            except (RuntimeError, ValueError) as e:
                raise HTTPException(422, str(e))

    dist = Path(__file__).parent / "static"
    if dist.exists():
        app.mount("/", StaticFiles(directory=dist, html=True))
    return app


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", type=Path)
    parser.add_argument("--workspace", type=Path)
    parser.add_argument("--port", type=int, default=8000)
    args = parser.parse_args()
    config = tomllib.loads(args.config.read_text()) if args.config else {}
    workspace = args.workspace
    if workspace is None:
        workspace = Path(config.get("workspace", "."))
        if args.config and not workspace.is_absolute():
            workspace = args.config.parent / workspace
    import uvicorn

    try:
        app = create_app(workspace, config.get("patterns"))
    except ValueError as e:
        parser.error(str(e))
    uvicorn.run(app, host="127.0.0.1", port=args.port)

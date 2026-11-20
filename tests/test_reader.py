import struct

import pytest
from fastapi.testclient import TestClient

from cobo_viewer import _bindings
from cobo_viewer.server import create_app


def frame(event=3, compact=False):
    body = (
        struct.pack(">H", 123)
        if compact
        else struct.pack(">I", (2 << 30) | (11 << 23) | (7 << 14) | 123)
    )
    h = bytearray(256)
    h[0] = 8
    h[1:4] = (2).to_bytes(3, "big")
    h[5:7] = (2 if compact else 1).to_bytes(2, "big")
    h[7] = 5
    struct.pack_into(">HHI", h, 8, 1, 2 if compact else 4, 1)
    struct.pack_into(">I", h, 22, event)
    return bytes(h) + body + bytes(256 - len(body))


def test_partial(tmp_path):
    p = tmp_path / "x.graw"
    p.write_bytes(frame())
    f = _bindings.GrawReader(str(p)).index()[0]
    t = _bindings.GrawReader(str(p)).read_at(f.offset, f.ordinal).traces[0]
    assert (f.event, f.offset, t.aget, t.channel) == (3, 0, 2, 11)
    assert t.samples[7] == 123 and sum(t.present) == 1


def test_compact(tmp_path):
    p = tmp_path / "x.graw"
    p.write_bytes(frame(compact=True))
    t = _bindings.GrawReader(str(p)).read().traces[0]
    assert t.channel == 0 and t.samples[0] == 123


def test_truncated(tmp_path):
    p = tmp_path / "x.graw"
    p.write_bytes(frame()[:-1])
    with pytest.raises(RuntimeError):
        _bindings.GrawReader(str(p)).index()


def test_server(tmp_path):
    d = tmp_path / "run_0001" / "mm0"
    d.mkdir(parents=True)
    (d / "x.graw").write_bytes(frame() + frame(7))
    c = TestClient(create_app(tmp_path))
    assert c.get("/api/runs").json() == ["0001"]
    assert c.get("/api/runs/0001/events").json() == [3, 7]
    t = c.get("/api/runs/0001/events/3").json()[0]
    assert t["samples"][7] == 123 and t["samples"][0] is None
    assert t["header"]["revision"] == 5
    assert t["header"]["item_count"] == 1
    assert len(t["header"]["hit_patterns"]) == 4


def test_server_custom_pattern(tmp_path):
    directory = tmp_path / "mm0" / "white" / "run_0000"
    directory.mkdir(parents=True)
    (directory / "x.graw").write_bytes(frame())
    client = TestClient(create_app(tmp_path, ["mm{cobo}/white/run_{run}"]))
    assert client.get("/api/runs").json() == ["0000"]
    assert client.get("/api/runs/0000/events").json() == [3]


@pytest.mark.parametrize(
    "patterns",
    [
        "mm<cobo>/white/run_<run>",
        [],
        ["mm<cobo>/white/run_<run>"],
        ["/mnt/mm{cobo}/white/run_{run}"],
        [123],
    ],
)
def test_server_invalid_patterns(tmp_path, patterns):
    with pytest.raises(ValueError, match="pattern"):
        create_app(tmp_path, patterns)


def test_sequential_header(tmp_path):
    p = tmp_path / "sequence.graw"
    data = bytearray(frame())
    data[4] = 9
    data[28:30] = (300).to_bytes(2, "big")
    data[30] = 5
    data[31:40] = bytes(range(9))
    data[67:69] = (17).to_bytes(2, "big")
    data[75:79] = (12345).to_bytes(4, "big")
    data[79:81] = (511).to_bytes(2, "big")
    p.write_bytes(data + frame(7))
    reader = _bindings.GrawReader(str(p))
    first = reader.read()
    assert first.header.data_source == 9
    assert first.header.read_offset == 300
    assert first.header.status == 5
    assert first.header.hit_patterns[0] == list(range(9))
    assert first.header.multiplicities[0] == 17
    assert first.header.window_out == 12345
    assert first.header.last_cells[0] == 511
    assert reader.read().header.event_id == 7
    assert reader.read() is None
    reader.rewind()
    assert reader.read().header.event_id == 3

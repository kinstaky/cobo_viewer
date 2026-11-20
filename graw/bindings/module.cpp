#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include "reader.h"

namespace nb = nanobind;

NB_MODULE(_bindings, m) {
	nb::class_<graw::FrameHeader>(m, "FrameHeader")
	    .def_ro("meta_type", &graw::FrameHeader::meta_type)
	    .def_ro("data_source", &graw::FrameHeader::data_source)
	    .def_ro("revision", &graw::FrameHeader::revision)
	    .def_ro("cobo_id", &graw::FrameHeader::cobo_id)
	    .def_ro("asad_id", &graw::FrameHeader::asad_id)
	    .def_ro("status", &graw::FrameHeader::status)
	    .def_ro("frame_size", &graw::FrameHeader::frame_size)
	    .def_ro("item_count", &graw::FrameHeader::item_count)
	    .def_ro("event_id", &graw::FrameHeader::event_id)
	    .def_ro("window_out", &graw::FrameHeader::window_out)
	    .def_ro("frame_type", &graw::FrameHeader::frame_type)
	    .def_ro("header_size", &graw::FrameHeader::header_size)
	    .def_ro("item_size", &graw::FrameHeader::item_size)
	    .def_ro("read_offset", &graw::FrameHeader::read_offset)
	    .def_ro("event_time", &graw::FrameHeader::event_time)
	    .def_ro("hit_patterns", &graw::FrameHeader::hit_patterns)
	    .def_ro("multiplicities", &graw::FrameHeader::multiplicities)
	    .def_ro("last_cells", &graw::FrameHeader::last_cells)
	    .def_ro("reserved", &graw::FrameHeader::reserved);
	nb::class_<graw::Trace>(m, "Trace")
	    .def_ro("aget", &graw::Trace::aget)
	    .def_ro("channel", &graw::Trace::channel)
	    .def_ro("samples", &graw::Trace::samples)
	    .def_ro("present", &graw::Trace::present);
	nb::class_<graw::Frame>(m, "Frame")
	    .def_ro("path", &graw::Frame::path)
	    .def_ro("offset", &graw::Frame::offset)
	    .def_ro("ordinal", &graw::Frame::ordinal)
	    .def_ro("header", &graw::Frame::header)
	    .def_ro("traces", &graw::Frame::traces)
	    .def_prop_ro("event", [](const graw::Frame &f) { return f.header.event_id; })
	    .def_prop_ro("time", [](const graw::Frame &f) { return f.header.event_time; })
	    .def_prop_ro("cobo", [](const graw::Frame &f) { return f.header.cobo_id; })
	    .def_prop_ro("asad", [](const graw::Frame &f) { return f.header.asad_id; });
	nb::class_<graw::GrawReader>(m, "GrawReader")
	    .def(nb::init<const std::string &>())
	    .def("read", &graw::GrawReader::Read, nb::call_guard<nb::gil_scoped_release>())
	    .def(
	        "index", &graw::GrawReader::Index, nb::call_guard<nb::gil_scoped_release>())
	    .def("read_at",
		     &graw::GrawReader::ReadAt,
		     nb::call_guard<nb::gil_scoped_release>())
	    .def("rewind", &graw::GrawReader::Rewind)
	    .def_prop_ro("file_size", &graw::GrawReader::FileSize);
}

#include "reader.h"

#include <array>
#include <cstring>
#include <stdexcept>

namespace graw {
namespace {

// Reverse the specified wire width, including the 24- and 48-bit fields.
uint64_t FromBigEndian(uint64_t value, unsigned bytes) {
	uint64_t result = 0;
	while (bytes--) {
		result = (result << 8) | (value & 0xff);
		value >>= 8;
	}
	return result;
}

void CheckLayout() {
	const uint16_t endian_probe = 1;
	const uint32_t partial_probe = 0xc0804001;
	const uint16_t compact_probe = 0xc001;
	PartialData partial;
	CompactData compact;
	std::memcpy(&partial, &partial_probe, sizeof(partial));
	std::memcpy(&compact, &compact_probe, sizeof(compact));
	if (*reinterpret_cast<const uint8_t *>(&endian_probe) != 1 || partial.aget != 3 ||
	    partial.channel != 1 || partial.time_bucket != 1 || partial.sample != 1 ||
	    compact.aget != 3 || compact.sample != 1) {
		throw std::runtime_error("Unsupported compiler/host GRAW bitfield layout");
	}
}

} // namespace

GrawReader::GrawReader(const std::string &path)
    : path_(path), handle_(path, std::ios::binary | std::ios::ate) {
	CheckLayout();
	if (!handle_)
		Fail("Cannot open file");
	const auto end = handle_.tellg();
	if (end < 0)
		Fail("Cannot determine file size");
	file_size_ = static_cast<uint64_t>(end);
	Rewind();
}

void GrawReader::Fail(const std::string &message) const {
	throw std::runtime_error(path_ + ":" + std::to_string(offset_) + ": " + message);
}

void GrawReader::ReadExact(void *destination, size_t bytes) {
	if (bytes && !handle_.read(static_cast<char *>(destination), bytes)) {
		Fail("Truncated GRAW data");
	}
}

void GrawReader::Rewind() {
	handle_.clear();
	handle_.seekg(0);
	if (!handle_)
		Fail("Cannot rewind file");
	offset_ = 0;
	ordinal_ = 0;
}

uint64_t GrawReader::FileSize() const {
	return file_size_;
}

Frame GrawReader::ReadHeader() {
	RawHeader raw{};
	ReadExact(&raw, sizeof(raw));
	Frame frame{};
	frame.path = path_;
	frame.offset = offset_;
	frame.ordinal = ordinal_;
	auto &header = frame.header;
	header.meta_type = raw.meta_type;
	header.frame_size = FromBigEndian(raw.frame_size, kFrameSizeBytes);
	header.data_source = raw.data_source;
	header.frame_type = FromBigEndian(raw.frame_type, sizeof(raw.frame_type));
	header.revision = raw.revision;
	header.header_size = FromBigEndian(raw.header_size, sizeof(raw.header_size));
	header.item_size = FromBigEndian(raw.item_size, sizeof(raw.item_size));
	header.item_count = FromBigEndian(raw.item_count, sizeof(raw.item_count));
	header.event_time = FromBigEndian(raw.event_time, kTimestampBytes);
	header.event_id = FromBigEndian(raw.event_id, sizeof(raw.event_id));
	header.cobo_id = raw.cobo_id;
	header.asad_id = raw.asad_id;
	header.read_offset = FromBigEndian(raw.read_offset, sizeof(raw.read_offset));
	header.status = raw.status;
	header.window_out = FromBigEndian(raw.window_out, sizeof(raw.window_out));
	for (unsigned aget = 0; aget < kAgets; ++aget) {
		header.hit_patterns.emplace_back(raw.hit_patterns[aget],
		                                 raw.hit_patterns[aget] + kHitPatternBytes);
		header.multiplicities.push_back(
		    FromBigEndian(raw.multiplicities[aget], sizeof(uint16_t)));
		header.last_cells.push_back(
		    FromBigEndian(raw.last_cells[aget], sizeof(uint16_t)));
	}
	header.reserved.assign(raw.reserved, raw.reserved + kReservedHeaderBytes);
	const auto frame_bytes = uint64_t(header.frame_size) * kBlockBytes;
	const auto expected_item_size =
	    header.frame_type == kPartialFrame ? sizeof(PartialData) : sizeof(CompactData);
	if (header.meta_type != kMetaType || header.revision != kRevision ||
	    header.header_size != kHeaderBlocks ||
	    (header.frame_type != kPartialFrame && header.frame_type != kCompactFrame) ||
	    header.item_size != expected_item_size || header.asad_id >= kAsads ||
	    frame_bytes < sizeof(RawHeader) || frame_bytes > file_size_ - offset_ ||
	    uint64_t(header.item_count) * header.item_size >
	        frame_bytes - sizeof(RawHeader)) {
		Fail("Invalid or unsupported frame header");
	}
	return frame;
}

void GrawReader::Decode(Frame &frame) {
	const auto &header = frame.header;
	body_.resize(size_t(header.item_count) * header.item_size);
	ReadExact(body_.data(), body_.size());
	std::array<int, kAgets * kChannels> lookup;
	lookup.fill(-1);
	std::array<unsigned, kAgets> counters{};
	frame.traces.reserve(kAgets * kChannels);
	const uint8_t *cursor = body_.data();
	for (uint32_t item = 0; item < header.item_count;
	     ++item, cursor += header.item_size) {
		unsigned aget, channel, bucket, sample;
		if (header.frame_type == kPartialFrame) {
			uint32_t word;
			std::memcpy(&word, cursor, sizeof(word));
			word = FromBigEndian(word, sizeof(word));
			PartialData data;
			std::memcpy(&data, &word, sizeof(data));
			aget = data.aget;
			channel = data.channel;
			bucket = data.time_bucket;
			sample = data.sample;
		} else {
			uint16_t word;
			std::memcpy(&word, cursor, sizeof(word));
			word = FromBigEndian(word, sizeof(word));
			CompactData data;
			std::memcpy(&data, &word, sizeof(data));
			aget = data.aget;
			channel = counters[aget] % kChannels;
			bucket = counters[aget]++ / kChannels;
			sample = data.sample;
		}
		if (channel >= kChannels || bucket >= kTimeBuckets)
			Fail("Invalid sample address");
		auto &index = lookup[aget * kChannels + channel];
		if (index < 0) {
			index = frame.traces.size();
			frame.traces.push_back({uint8_t(aget),
			                        uint8_t(channel),
			                        std::vector<uint16_t>(kTimeBuckets),
			                        std::vector<uint8_t>(kTimeBuckets)});
		}
		auto &trace = frame.traces[index];
		if (trace.present[bucket])
			Fail("Duplicate sample");
		trace.samples[bucket] = sample;
		trace.present[bucket] = 1;
	}
}

std::optional<Frame> GrawReader::Read() {
	if (offset_ == file_size_)
		return std::nullopt;
	auto frame = ReadHeader();
	Decode(frame);
	offset_ += uint64_t(frame.header.frame_size) * kBlockBytes;
	++ordinal_;
	handle_.seekg(offset_);
	if (!handle_)
		Fail("Cannot advance to next frame");
	return frame;
}

std::vector<Frame> GrawReader::Index() {
	Rewind();
	std::vector<Frame> frames;
	while (offset_ < file_size_) {
		auto frame = ReadHeader();
		offset_ += uint64_t(frame.header.frame_size) * kBlockBytes;
		++ordinal_;
		frames.push_back(std::move(frame));
		handle_.seekg(offset_);
		if (!handle_)
			Fail("Cannot advance to next header");
	}
	Rewind();
	return frames;
}

Frame GrawReader::ReadAt(uint64_t offset, uint64_t ordinal) {
	if (offset >= file_size_)
		Fail("Frame offset is outside file");
	handle_.clear();
	handle_.seekg(offset);
	if (!handle_)
		Fail("Cannot seek to frame");
	offset_ = offset;
	ordinal_ = ordinal;
	return std::move(*Read());
}

} // namespace graw

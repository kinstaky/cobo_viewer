#pragma once

#include <cstdint>
#include <cstddef>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace graw {

constexpr unsigned kBlockBytes = 256;
constexpr unsigned kHeaderBlocks = 1;
constexpr unsigned kFrameSizeBytes = 3;
constexpr unsigned kTimestampBytes = 6;
constexpr unsigned kRevision = 5;
constexpr unsigned kMetaType = 8;
constexpr unsigned kPartialFrame = 1;
constexpr unsigned kCompactFrame = 2;
constexpr unsigned kAsads = 4;
constexpr unsigned kAgets = 4;
constexpr unsigned kChannels = 68;
constexpr unsigned kTimeBuckets = 512;
constexpr unsigned kHitPatternBytes = 9;
constexpr unsigned kReservedHeaderBytes = 169;

// Packed wire header for revision 5. Multibyte fields are big endian on disk.
// The compiler's bitfield layout is checked at construction before reading data.
#pragma pack(push, 1)
struct RawHeader {
	uint8_t meta_type;
	uint32_t frame_size : 24;
	uint8_t data_source;
	uint16_t frame_type;
	uint8_t revision;
	uint16_t header_size;
	uint16_t item_size;
	uint32_t item_count;
	uint64_t event_time : 48;
	uint32_t event_id;
	uint8_t cobo_id;
	uint8_t asad_id;
	uint16_t read_offset;
	uint8_t status;
	uint8_t hit_patterns[kAgets][kHitPatternBytes];
	uint16_t multiplicities[kAgets];
	uint32_t window_out;
	uint16_t last_cells[kAgets];
	uint8_t reserved[kReservedHeaderBytes];
};
#pragma pack(pop)

// Bitfields describe host-order data words, after conversion from wire order.
struct PartialData {
	uint32_t sample : 12;
	uint32_t reserved : 2;
	uint32_t time_bucket : 9;
	uint32_t channel : 7;
	uint32_t aget : 2;
};
struct CompactData {
	uint16_t sample : 12;
	uint16_t reserved : 2;
	uint16_t aget : 2;
};
static_assert(sizeof(RawHeader) == kBlockBytes, "Unsupported packed header layout");
static_assert(offsetof(RawHeader, frame_type) == 5 &&
                  offsetof(RawHeader, event_id) == 22 &&
                  offsetof(RawHeader, hit_patterns) == 31 &&
                  offsetof(RawHeader, window_out) == 75 &&
                  offsetof(RawHeader, reserved) == 87,
              "Unsupported header field offsets");
static_assert(sizeof(PartialData) == sizeof(uint32_t),
              "Unsupported partial word layout");
static_assert(sizeof(CompactData) == sizeof(uint16_t),
              "Unsupported compact word layout");

// Host-order metadata. Raw hit-pattern bytes and reserved bytes are preserved.
struct FrameHeader {
	uint8_t meta_type, data_source, revision, cobo_id, asad_id, status;
	uint32_t frame_size, item_count, event_id, window_out;
	uint16_t frame_type, header_size, item_size, read_offset;
	uint64_t event_time;
	std::vector<std::vector<uint8_t>> hit_patterns;
	std::vector<uint16_t> multiplicities, last_cells;
	std::vector<uint8_t> reserved;
};

struct Trace {
	uint8_t aget, channel;
	std::vector<uint16_t> samples;
	std::vector<uint8_t> present;
};
struct Frame {
	std::string path;
	uint64_t offset, ordinal;
	FrameHeader header;
	std::vector<Trace> traces;
};

class GrawReader {
public:
	explicit GrawReader(const std::string &path);
	GrawReader(const GrawReader &) = delete;
	GrawReader &operator=(const GrawReader &) = delete;

	// Sequential reads consume a header once, then its payload and padding.
	// A clean end of file returns nullopt; malformed/truncated frames throw.
	std::optional<Frame> Read();
	std::vector<Frame> Index();
	Frame ReadAt(uint64_t offset, uint64_t ordinal);
	void Rewind();
	uint64_t FileSize() const;

private:
	void ReadExact(void *destination, size_t bytes);
	Frame ReadHeader();
	void Decode(Frame &frame);
	[[noreturn]] void Fail(const std::string &message) const;

	std::string path_;
	std::ifstream handle_;
	uint64_t file_size_ = 0;
	uint64_t offset_ = 0;
	uint64_t ordinal_ = 0;
	std::vector<uint8_t> body_;
};

} // namespace graw

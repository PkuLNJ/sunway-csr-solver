#ifndef LNJ_LOSSLESS_CAPTURE_WRITER_H
#define LNJ_LOSSLESS_CAPTURE_WRITER_H
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>
namespace lnjcapture {
// Never evaluates array elements. Caller owns lifetime and supplies exact bytes.
class CaptureWriter {
    FILE* payload_; FILE* index_; std::string directory_;
    std::uint64_t offset_, entries_; bool complete_;
    std::set<std::string> keys_;
    CaptureWriter(const CaptureWriter&) = delete;
    CaptureWriter& operator=(const CaptureWriter&) = delete;
public:
    CaptureWriter(const std::string& existingDirectory, unsigned rank);
    ~CaptureWriter();
    void write(const std::string& stage, const std::string& tag,
               const std::string& dtype, unsigned width,
               std::uint64_t count, const void* raw);
    void checkpoint();
    void finish();
};
}
#endif

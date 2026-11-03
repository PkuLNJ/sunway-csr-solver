#include "CaptureWriter.H"
#include <cerrno>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <sstream>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#define LNJ_OPEN _open
#define LNJ_FDOPEN _fdopen
#define LNJ_CLOSE _close
#define LNJ_BINARY _O_BINARY
#else
#include <unistd.h>
#define LNJ_OPEN open
#define LNJ_FDOPEN fdopen
#define LNJ_CLOSE close
#define LNJ_BINARY 0
#endif
namespace lnjcapture {
static FILE* exclusive(const std::string& path) {
    int fd=LNJ_OPEN(path.c_str(), O_WRONLY|O_CREAT|O_EXCL|LNJ_BINARY, 0600);
    if(fd<0) throw std::runtime_error("capture exclusive create failed: "+path+": "+std::strerror(errno));
    FILE* f=LNJ_FDOPEN(fd,"wb");
    if(!f) { LNJ_CLOSE(fd); throw std::runtime_error("capture fdopen failed: "+path); }
    return f;
}
static void checkedFlush(FILE* f) {
    if(std::fflush(f)!=0 || std::ferror(f)) throw std::runtime_error("capture flush failed");
}
static void token(const std::string& s) {
    if(s.empty() || s.find_first_of("\t\r\n")!=std::string::npos)
        throw std::runtime_error("capture invalid stage/tag");
}
CaptureWriter::CaptureWriter(const std::string& dir,unsigned rank):payload_(0),index_(0),directory_(dir),offset_(0),entries_(0),complete_(false) {
    std::ostringstream prefix; prefix<<dir<<"/rank-"<<rank;
    directory_=prefix.str();
    // If construction fails, retain partial evidence; never delete or reuse it.
    payload_=exclusive(directory_+".bin");
    try { index_=exclusive(directory_+".tsv"); }
    catch(...) { std::fclose(payload_); payload_=0; throw; }
    std::uint32_t endian=1;
    if(std::fprintf(index_,"LNJ_CAPTURE_V1\t%s\n",(*reinterpret_cast<unsigned char*>(&endian)==1)?"little":"big")<0)
        throw std::runtime_error("capture header write failed");
}
CaptureWriter::~CaptureWriter() { if(payload_)std::fclose(payload_); if(index_)std::fclose(index_); }
void CaptureWriter::write(const std::string& stage,const std::string& tag,const std::string& dtype,unsigned width,std::uint64_t count,const void* raw) {
    if(complete_) throw std::runtime_error("capture already complete");
    token(stage); token(tag);
    if(!width || (dtype!="f" && dtype!="i" && dtype!="u" && dtype!="bytes")) throw std::runtime_error("capture invalid dtype/width");
    if((dtype=="f" && width!=4 && width!=8) || ((dtype=="i"||dtype=="u") && width!=1 && width!=2 && width!=4 && width!=8)) throw std::runtime_error("capture unsupported scalar width");
    if(count>std::numeric_limits<std::uint64_t>::max()/width) throw std::runtime_error("capture size overflow");
    const std::uint64_t bytes=count*width;
    if(bytes>std::numeric_limits<std::uint64_t>::max()-offset_) throw std::runtime_error("capture offset overflow");
    if(bytes && !raw) throw std::runtime_error("capture null array");
    const std::string key=stage+"\t"+tag;
    if(!keys_.insert(key).second) throw std::runtime_error("capture duplicate stage/tag");
    const unsigned char* p=static_cast<const unsigned char*>(raw);
    std::uint64_t remaining=bytes;
    while(remaining) {
        const std::size_t chunk=static_cast<std::size_t>(remaining>1048576?1048576:remaining);
        if(std::fwrite(p,1,chunk,payload_)!=chunk) throw std::runtime_error("capture payload write failed");
        p+=chunk; remaining-=chunk;
    }
    if(std::fprintf(index_,"%s\t%s\t%s\t%u\t%llu\t%llu\t%llu\n",stage.c_str(),tag.c_str(),dtype.c_str(),width,(unsigned long long)count,(unsigned long long)offset_,(unsigned long long)bytes)<0) throw std::runtime_error("capture index write failed");
    offset_+=bytes; ++entries_;
}
void CaptureWriter::checkpoint() {
    if(complete_)throw std::runtime_error("checkpoint after completion");
    checkedFlush(payload_);checkedFlush(index_);
}
void CaptureWriter::finish() {
    if(complete_) throw std::runtime_error("capture finish called twice");
    checkedFlush(payload_); checkedFlush(index_);
    // Completion exists only after both streams have successfully flushed/closed.
    int p=std::fclose(payload_); payload_=0;
    int i=std::fclose(index_); index_=0;
    if(p || i) throw std::runtime_error("capture close failed");
    FILE* marker=exclusive(directory_+".complete");
    if(std::fprintf(marker,"LNJ_CAPTURE_COMPLETE_V1\t%llu\t%llu\n",(unsigned long long)entries_,(unsigned long long)offset_)<0) { std::fclose(marker); throw std::runtime_error("capture marker write failed"); }
    try { checkedFlush(marker); } catch(...) { std::fclose(marker); throw; }
    if(std::fclose(marker)!=0) throw std::runtime_error("capture marker close failed");
    complete_=true;
}
}

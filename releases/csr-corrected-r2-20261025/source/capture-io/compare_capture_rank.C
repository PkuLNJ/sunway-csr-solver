// C++14 standalone streaming comparator. No MPI or OpenFOAM dependencies.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#define OP _open
#define FDOPEN _fdopen
#else
#include <unistd.h>
#define OP open
#define FDOPEN fdopen
#endif
struct Rec { std::string stage,tag,type; unsigned width; std::uint64_t count,offset,size; bool little; };
struct Norm {
    long double scale=0,ssq=1;
    void add(long double v) {v=std::fabs(v);if(!v)return;if(scale<v){ssq=1+ssq*(scale/v)*(scale/v);scale=v;}else ssq+=(v/scale)*(v/scale);}
};
static void fail(const std::string&s){throw std::runtime_error(s);}
static std::vector<std::string> split(const std::string&s){std::vector<std::string> v;std::stringstream f(s);std::string x;while(std::getline(f,x,'\t'))v.push_back(x);return v;}
static std::uint64_t number(const std::string&s){if(s.empty()||s[0]=='-')fail("invalid integer");std::size_t n;auto v=std::stoull(s,&n);if(n!=s.size())fail("invalid integer suffix");return v;}
static std::map<std::string,Rec> index(const std::string&prefix){
    std::ifstream f(prefix+".tsv",std::ios::binary);if(!f)fail("missing index "+prefix);
    std::string line;std::getline(f,line);if(!line.empty()&&line.back()=='\r')line.pop_back();
    if(line!="LNJ_CAPTURE_V1\tlittle"&&line!="LNJ_CAPTURE_V1\tbig")fail("invalid header");
    bool little=line=="LNJ_CAPTURE_V1\tlittle";
    std::map<std::string,Rec> out;std::uint64_t end=0;
    while(std::getline(f,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        auto p=split(line);if(p.size()!=7)fail("bad index row");
        auto w=number(p[3]);Rec r{p[0],p[1],p[2],static_cast<unsigned>(w),number(p[4]),number(p[5]),number(p[6]),little};
        if(r.stage.empty()||r.tag.empty()||!w||w>std::numeric_limits<unsigned>::max())fail("invalid schema");
        if(r.type!="f"&&r.type!="i"&&r.type!="u"&&r.type!="bytes")fail("invalid dtype");
        if(r.type=="f"&&w!=4&&w!=8)fail("invalid float width");
        if((r.type=="i"||r.type=="u")&&w!=1&&w!=2&&w!=4&&w!=8)fail("invalid int width");
        if(r.count>std::numeric_limits<std::uint64_t>::max()/w||r.size!=r.count*w||r.offset!=end||r.size>std::numeric_limits<std::uint64_t>::max()-end)fail("size/overlap error");
        if(!out.emplace(r.stage+"\t"+r.tag,r).second)fail("duplicate stage/tag");
        end+=r.size;
    }
    std::ifstream m(prefix+".complete");std::getline(m,line);if(!line.empty()&&line.back()=='\r')line.pop_back();auto p=split(line);
    if(!m||p.size()!=3||p[0]!="LNJ_CAPTURE_COMPLETE_V1"||number(p[1])!=out.size()||number(p[2])!=end)fail("incomplete capture");
    std::ifstream b(prefix+".bin",std::ios::binary|std::ios::ate);if(!b||static_cast<std::uint64_t>(b.tellg())!=end)fail("truncated/trailing payload");return out;
}
static std::uint64_t bits(const unsigned char*p,unsigned w,bool little){std::uint64_t u=0;for(unsigned i=0;i<w;++i)u|=std::uint64_t(p[i])<<(8*(little?i:w-1-i));return u;}
static long double value(const unsigned char*p,const Rec&r){
    if(r.type=="bytes")return *p;
    std::uint64_t u=bits(p,r.width,r.little);
    if(r.type=="u")return static_cast<long double>(u);
    if(r.type=="i"){
        if(r.width<8&&(u&(std::uint64_t(1)<<(r.width*8-1))))u|=(~std::uint64_t(0))<<(r.width*8);
        std::int64_t s;std::memcpy(&s,&u,8);return static_cast<long double>(s);
    }
    if(r.width==4){std::uint32_t z=static_cast<std::uint32_t>(u);float x;std::memcpy(&x,&z,4);return x;}
    double x;std::memcpy(&x,&u,8);return x;
}
static std::string numeric(long double v){std::ostringstream s;s<<std::setprecision(std::numeric_limits<long double>::max_digits10)<<v;return s.str();}
static bool sameShape(const Rec&a,const Rec&b){return a.type==b.type&&a.width==b.width&&a.count==b.count;}
static std::string shape(const Rec&r){return r.type+":"+std::to_string(r.width)+":"+std::to_string(r.count);}
int main(int argc,char**argv){try{
    if(argc!=5&&argc!=6)fail("usage: compare_capture_rank ARM_A ARM_B RANK OUTPUT.tsv [--allow-schema-differences]");
    bool allow=argc==6&&std::string(argv[5])=="--allow-schema-differences";
    if(argc==6&&!allow)fail("unknown option");
    auto rank=number(argv[3]);std::string suffix="/rank-"+std::to_string(rank),ap=argv[1]+suffix,bp=argv[2]+suffix;
    auto ai=index(ap),bi=index(bp);
    if(!allow){
        if(ai.size()!=bi.size())fail("array key count mismatch");
        for(const auto&kv:ai){auto j=bi.find(kv.first);if(j==bi.end())fail("missing array key");if(!sameShape(kv.second,j->second))fail("array schema mismatch");}
    }
    int flags=O_WRONLY|O_CREAT|O_EXCL;
#ifdef _WIN32
    flags|=_O_BINARY;
#endif
    int fd=OP(argv[4],flags,0600);if(fd<0)fail("exclusive output create failed");FILE*out=FDOPEN(fd,"wb");if(!out)fail("fdopen failed");
    if(allow){
        std::string path=std::string(argv[4])+".schema.tsv";int sf=OP(path.c_str(),flags,0600);if(sf<0)fail("exclusive schema create failed");FILE*s=FDOPEN(sf,"wb");if(!s)fail("schema fdopen failed");
        std::fprintf(s,"LNJ_COMPARE_SCHEMA_V1\t%llu\nstatus\tstage\ttag\tleft_shape\tright_shape\n",(unsigned long long)rank);
        std::uint64_t n=0;
        for(const auto&kv:ai){auto j=bi.find(kv.first);if(j==bi.end()){std::fprintf(s,"LEFT_ONLY\t%s\t%s\t%s\t-\n",kv.second.stage.c_str(),kv.second.tag.c_str(),shape(kv.second).c_str());++n;}else if(!sameShape(kv.second,j->second)){std::fprintf(s,"SHAPE_MISMATCH\t%s\t%s\t%s\t%s\n",kv.second.stage.c_str(),kv.second.tag.c_str(),shape(kv.second).c_str(),shape(j->second).c_str());++n;}}
        for(const auto&kv:bi)if(ai.find(kv.first)==ai.end()){std::fprintf(s,"RIGHT_ONLY\t%s\t%s\t-\t%s\n",kv.second.stage.c_str(),kv.second.tag.c_str(),shape(kv.second).c_str());++n;}
        if(std::fprintf(s,"COMPLETE\t%llu\n",(unsigned long long)n)<0||std::fflush(s)||std::ferror(s))fail("schema flush failed");
        if(std::fclose(s))fail("schema close failed");
    }
    std::fprintf(out,"LNJ_COMPARE_RANK_V1\t%llu\n",(unsigned long long)rank);
    std::fprintf(out,"stage\ttag\tdtype\twidth\tcount\tbyte_equal\tfinite\tnonfinite_left\tnonfinite_right\tnonfinite_difference\tfirst_index\tfirst_left\tfirst_right\tmaxabs\tleft_scale\tleft_ssq\tright_scale\tright_ssq\tdiff_scale\tdiff_ssq\tmax_index\tmax_left\tmax_right\n");
    std::ifstream af(ap+".bin",std::ios::binary),bf(bp+".bin",std::ios::binary);
    std::vector<unsigned char> aa(1048576),bb(1048576);std::uint64_t rows=0;
    for(const auto&kv:ai){auto peer=bi.find(kv.first);if(peer==bi.end()||!sameShape(kv.second,peer->second))continue;const Rec&a=kv.second;const Rec&b=peer->second;af.seekg(a.offset);bf.seekg(b.offset);
        bool equal=true,finite=true,first=false;std::uint64_t nl=0,nr=0,nd=0,firstIndex=0,maxIndex=0,base=0,remain=a.size;long double firstL=0,firstR=0,maxL=0,maxR=0,maxabs=0;Norm ln,rn,dn;
        const unsigned step=a.type=="bytes"?1:a.width;
        while(remain){std::size_t n=static_cast<std::size_t>(std::min<std::uint64_t>(remain,(1048576/step)*step));if(!n)fail("record width exceeds buffer");
            af.read(reinterpret_cast<char*>(aa.data()),n);bf.read(reinterpret_cast<char*>(bb.data()),n);if(static_cast<std::size_t>(af.gcount())!=n||static_cast<std::size_t>(bf.gcount())!=n)fail("short payload read");equal=equal&&(std::memcmp(aa.data(),bb.data(),n)==0);
            for(std::size_t i=0;i<n;i+=step){
                long double x=value(aa.data()+i,a),y=value(bb.data()+i,b),d=x-y;
                bool different=x!=y;
                if(a.type!="f") {
                    auto ux=bits(aa.data()+i,step,a.little),uy=bits(bb.data()+i,step,b.little);
                    different=ux!=uy;
                    if(a.type=="i"){const auto sign=std::uint64_t(1)<<(step*8-1);ux^=sign;uy^=sign;}
                    d=ux>=uy?static_cast<long double>(ux-uy):-static_cast<long double>(uy-ux);
                }
                if(different&&!first){first=true;firstIndex=base+i/step;firstL=x;firstR=y;}
                bool xf=std::isfinite(x),yf=std::isfinite(y);if(!xf)++nl;if(!yf)++nr;
                if(!xf||!yf){finite=false;continue;}ln.add(x);rn.add(y);
                if(!std::isfinite(d)){finite=false;++nd;continue;}dn.add(d);
                if(std::fabs(d)>maxabs){maxabs=std::fabs(d);maxIndex=base+i/step;maxL=x;maxR=y;}
            }
            base+=n/step;remain-=n;
        }
        std::ostringstream row;row<<a.stage<<'\t'<<a.tag<<'\t'<<a.type<<'\t'<<a.width<<'\t'<<a.count<<'\t'<<equal<<'\t'<<finite<<'\t'<<nl<<'\t'<<nr<<'\t'<<nd<<'\t'<<(first?std::to_string(firstIndex):"-")<<'\t'<<(first?numeric(firstL):"-")<<'\t'<<(first?numeric(firstR):"-")<<'\t'<<numeric(maxabs)<<'\t'<<numeric(ln.scale)<<'\t'<<numeric(ln.ssq)<<'\t'<<numeric(rn.scale)<<'\t'<<numeric(rn.ssq)<<'\t'<<numeric(dn.scale)<<'\t'<<numeric(dn.ssq)<<'\t'<<(maxabs?std::to_string(maxIndex):"-")<<'\t'<<(maxabs?numeric(maxL):"-")<<'\t'<<(maxabs?numeric(maxR):"-")<<'\n';
        auto s=row.str();if(std::fwrite(s.data(),1,s.size(),out)!=s.size())fail("stats write failed");++rows;
    }
    if(std::fprintf(out,"COMPLETE\t%llu\n",(unsigned long long)rows)<0||std::fflush(out)||std::ferror(out))fail("stats flush failed");
    if(std::fclose(out))fail("stats close failed");
    return 0;
}catch(const std::exception&e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}}

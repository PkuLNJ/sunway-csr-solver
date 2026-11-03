#include <mpi.h>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
namespace { int savedRank=-1; }
extern "C" int __real_MPI_Init_thread(int*,char***,int,int*);
extern "C" int __wrap_MPI_Init_thread(int* argc,char*** argv,int required,int* provided)
{
    int rc=__real_MPI_Init_thread(argc,argv,required,provided);
    if(rc==MPI_SUCCESS)MPI_Comm_rank(MPI_COMM_WORLD,&savedRank);
    return rc;
}
extern "C" int __real_MPI_Init(int*,char***);
extern "C" int __wrap_MPI_Init(int* argc,char*** argv)
{
    int rc=__real_MPI_Init(argc,argv);
    if(rc==MPI_SUCCESS)MPI_Comm_rank(MPI_COMM_WORLD,&savedRank);
    return rc;
}
extern "C" int __real_main(int,char**);
extern "C" int __wrap_main(int argc,char** argv)
{
    int rc=__real_main(argc,argv);
    const char* dir=std::getenv("LNJ_AMR_CAPTURE_DIR");
    if(dir&&*dir)
    {
        if(savedRank<0)return 78;
        char path[4096];
        int n=std::snprintf(path,sizeof(path),"%s/exit-rank-%d.txt",dir,savedRank);
        if(n<0||n>=int(sizeof(path)))return 78;
        int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);
        if(fd<0)return 78;
        char data[160];n=std::snprintf(data,sizeof(data),"RANK=%d\nSOLVER_MAIN_RETURN_CODE=%d\n",savedRank,rc);
        bool ok=write(fd,data,n)==n;
        if(close(fd)!=0)ok=false;
        if(!ok)return 78;
    }
    return rc;
}

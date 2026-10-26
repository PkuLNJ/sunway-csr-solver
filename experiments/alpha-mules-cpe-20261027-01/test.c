#include "src/lnjMulesParam.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
int lnjTestTid;
void lnjMules_CPE(lnjMulesParam*);
static double mn(double a,double b){return a<b?a:b;}
static double mx(double a,double b){return a>b?a:b;}
int main(void){
 int sizes[]={0,1,63,64,65,127,128,129,4097,17003};
 for(int test=0;test<10;++test){int n=sizes[test];
 double *x=calloc(n+1,8),*y=calloc(n+1,8),*a=calloc(n+1,8),*b=calloc(n+1,8),*c=calloc(n+1,8),*d=calloc(n+1,8),*rx=calloc(n+1,8),*ry=calloc(n+1,8);
 int64_t *o=calloc(n+1,8),*nb=calloc(n+1,8);
 for(int i=0;i<n;++i){x[i]=(i%19)*.12;y[i]=(i%13)*.17;a[i]=-(i%5)*.09;b[i]=(i%7)*.1;c[i]=-(i%3)*.11;d[i]=(i%11)*.13;
 rx[i]=mx(mn((x[i]+a[i])/(b[i]+1e-150),1),0);ry[i]=mx(mn((y[i]+c[i])/(d[i]+1e-150),1),0);}
 lnjMulesParam p={0};p.count=n;p.cells=n;p.tiny=1e-150;p.out0=x;p.out1=y;p.a=a;p.b=b;p.c=c;p.d=d;
 for(lnjTestTid=0;lnjTestTid<64;++lnjTestTid)lnjMules_CPE(&p);
 assert(!memcmp(x,rx,n*8)&&!memcmp(y,ry,n*8));
 for(int i=0;i<n;++i){a[i]=(i%3)-1;b[i]=x[i];c[i]=y[i];x[i]=.8;o[i]=(i*7919LL)%n;nb[i]=(i*3571LL+1)%n;}
 for(int i=0;i<n;++i)rx[i]=a[i]>0?mn(x[i],mn(b[o[i]],c[nb[i]])):mn(x[i],mn(c[o[i]],b[nb[i]]));
 p.mode=1;p.owner=o;p.neighbour=nb;
 for(lnjTestTid=0;lnjTestTid<64;++lnjTestTid)lnjMules_CPE(&p);
 assert(!memcmp(x,rx,n*8));free(x);free(y);free(a);free(b);free(c);free(d);free(rx);free(ry);free(o);free(nb);
 }puts("PASS bitwise reference, 10 sizes, 64 partitions, cache collisions and DMA tails");return 0;
}

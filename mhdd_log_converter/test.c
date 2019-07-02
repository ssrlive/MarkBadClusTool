#include<stdio.h>
#include<stdlib.h>
#include<assert.h>
#include<string.h>

int main(int argc, const char *argv[]) {
    char buffer[256] = { 0 };
    int count = 0;
    FILE *fp = fopen("mhdd.log","r");
    FILE *fp2 = fopen("badlist.txt","w");
    if (fp == NULL) {
        printf("Î´ÕÒµ½ mhdd.log ÎÄ¼þ£¡\n");
        system("PAUSE");
        exit(0);
    }
    assert( fp2 != NULL);
    while( fgets(buffer, sizeof(buffer), fp) != NULL ) {
        char buffer2[256] = { 0 };
        long long x;
        char *p = strstr( buffer,"\xfe\x20LBA");
        if( p == NULL) {
            continue;
        }
        p+=6;
        sscanf(p,"%s%lld", buffer2, &x);
        fprintf(fp2, "%lld\n", x);
        count++;
    }
    printf("count = %d\n", count );
    fclose( fp );
    fclose( fp2 );
    system("PAUSE");
    return 0;
}

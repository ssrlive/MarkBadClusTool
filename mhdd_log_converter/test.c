#include<stdio.h>
#include<stdlib.h>
#include<assert.h>
#include<string.h>

void deal_with_mhdd_log(void);
void deal_with_diskgenius_log(void);

int main(int argc, const char *argv[]) {
#if 0
    deal_with_mhdd_log();
#else
    deal_with_diskgenius_log();
#endif
    system("PAUSE");
    return 0;
}

void deal_with_mhdd_log(void) {
    char buffer[256] = { 0 };
    int count = 0;
    FILE *fp = fopen("mhdd.log","r");
    FILE *fp2 = fopen("badlist.txt","w");
    if (fp == NULL) {
        printf("Can NOT find file \"mhdd.log\".\n");
        return;
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
}

void deal_with_diskgenius_log(void) {
#define SECTORS_PER_CYLINDER 16065 /* 每柱面上的扇区数 */
    char buffer[256] = { 0 };
    int count = 0;
    FILE *fp = fopen("diskgenius.txt","r");
    FILE *fp2 = fopen("badlist.txt","w");
    if (fp == NULL) {
        printf("Can NOT find file \"diskgenius.txt\".\n");
        return;
    }
    assert(fp2 != NULL);
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        char buffer2[256] = { 0 };
        long long x = 0;
        char *p = buffer;
        sscanf(p,"%lld", &x);
        if (x > 0) {
            int i = 0;
            for (i = 0; i < SECTORS_PER_CYLINDER; ++i) {
                fprintf(fp2, "%lld\n", x * SECTORS_PER_CYLINDER + i);
            }
            count++;
        }
    }
    printf("count = %d\n", count);
    fclose(fp);
    fclose(fp2);
}

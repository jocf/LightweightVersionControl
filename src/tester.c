#include <assert.h>
#include "./lib/svc.h"

int main(int argc, char **argv) {
    void *helper = svc_init();
    printf("%d\n",hash_file(helper,"hello.py"));
    
    printf("%d\n",svc_add(helper,"hello.py"));
    printf("%d\n",svc_add(helper,"Tests/diff.txt"));
    printf("%d\n",svc_add(helper,"hello.py"));
    printf("%d\n",svc_add(helper,"hello.y"));
    printf("%d\n",svc_add(helper,NULL));

    
    cleanup(helper);
    
    return 0;
}



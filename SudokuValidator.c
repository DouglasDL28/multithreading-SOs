/* 
Universidad del Valle de Guatemala
Sistemas Operativos
Douglas de León Molina - 18037

Para compilarlo con GCC:
`gcc SudokuValidator.c -o sudoku -fopenmp -lrt`

Para ejecutarlo con sudoku de prueba:
`./sudoku "sudoku.21"`
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/syscall.h>

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

/* GLOBAL VARS */
int array[9][9];
int res;


void validate_columns() {
    omp_set_nested(1);
    omp_set_num_threads(9);
    /* validate every column of 9x9 array */
    int col, row;

    #pragma omp parallel for private(col, row) /* schedule(dynamic, 1) */
    for (col = 0; col < 9; col++) {
        printf("Thread id: %d\n", (int)syscall(SYS_gettid));
        int sum;
        sum = 0;
        /* validar filas */
        for (row = 0; row < 9; row++) {
            sum += (int) array[row][col];
        }
        res = (int)(res & (int) (sum==45));

    }
    pthread_exit(0);
}


void validate_rows() {
    omp_set_nested(1);
    omp_set_num_threads(9);
    /* validate every row or a 9x9 array */
    int row, col;
    #pragma omp parallel for private(col, row) schedule(dynamic, 1)
    for (row = 0; row < 9; row++) {
        int sum;
        sum = 0;
        /* validar columnas */
        for (col = 0; col < 9; col++) {
            sum += (int) array[row][col];
        }
        res = (int)(res & (int) (sum==45));
    }

}

/* row, col in {1,4,7} */
void validate_subarray(int row, int col) {
    omp_set_nested(1);
    omp_set_num_threads(9);
    /* validate 3x3 array in a 9x9 array */
    int x, y;
    int sum;

    sum = 0;
    #pragma omp parallel for private(x, y) schedule(dynamic, 1)
    for (y = row; y < row+3; y++) {
        for (x = col; x < col+3; x++) {
            sum += (int) array[y][x];
        }
    }
    res = (int) (res & (int) (sum==45));
}


void* thread_validation() {
    printf("Thread function id: %d\n", (int)syscall(SYS_gettid));
    validate_columns();
    pthread_exit(0);
}


int main(int argc, char const *argv[]) {
    omp_set_nested(1);
    // omp_set_num_threads(1);
    /* receives filepath of sudoku solution */
    int fd;
    int i, j;
    char *ptr;
    struct stat sb;

    res = 1; // init result to one

    /* open filepath */
    fd = open((char *)argv[1], O_RDWR);
    if (fd < 0)
        handle_error("open");

    /* obtain file size */
    if (fstat(fd, &sb) == -1)
        handle_error("fstat");

    /* map memory */
    ptr = (char *) mmap(0, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr < 0)
        handle_error("mmap");

    close(fd); // close file descriptor

    int nrow;
    nrow = 0;

    /* fill 9x9 array with file values */
    for (i = 0; i < sb.st_size; i++) {
        if ((i > 0) & (int) (i % 9)==0) {
            nrow++;
        }

        array[nrow][i % 9] = (int) ptr[i];
    }

    int index;
    int indexes[3];
    indexes[0] = 0;
    indexes[1] = 3;
    indexes[2] = 6;

    /* check 3x3 subarrays */

    #pragma omp parallel for private(i, j) schedule(dynamic, 1)
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            int y, x;
            y = indexes[i];
            x = indexes[j];
            validate_subarray(y, x);
        }
    }

    pid_t pid;
    pid_t fpid;

    pid = getpid();
    printf("Parent id: %d\n", (int)pid);

    fpid = fork();
    if (fpid == -1)
        handle_error("fork");
    
    /* CHILD PROCESS */
    if (fpid == 0) {
        int p;
        char id[5];

        snprintf(id, sizeof(id), "%d", (int) getppid());

        p = execlp("/bin/ps", "ps", "-lLf", "p", id, NULL);
        if (p == -1)
            handle_error("execlp");
    }
    /* PARENT PROCESS */
    if (fpid > 0) {
        pthread_t tid;
        pthread_attr_t attr;
        pid_t fpid2;

        /* set the default attributes of the thread */
        pthread_attr_init(&attr);
        /* create the thread */
        pthread_create(&tid, &attr, thread_validation, NULL);
        /* wait for the thread to exit */
        pthread_join(tid, NULL);

        printf("Get thread id: %d\n", (int) syscall(SYS_gettid)); // get thread id.
        wait(NULL); // wait for child
        validate_rows();

        if (res == 1) {
            printf("Successful sukodu solution\n");
        }
        else {
            printf("Not a solution\n");
        }

        fpid2 = fork();
        if (fpid2 == -1)
            handle_error("fork");

        /* CHILD */
        if (fpid2 == 0) {
            int p2;
            char id[5];

            snprintf(id, sizeof(id), "%d", (int)getppid());
            
            p2 = execlp("/bin/ps", "ps","-lLf", "p", id, NULL);
            if (p2 == -1)
                handle_error("execlp2");

        }
        /* PARENT */
        else {
            wait(NULL); // wait for child
            return 0;
        }
    }


    return 0;
}

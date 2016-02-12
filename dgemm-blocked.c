const char* dgemm_desc = "Simple blocked dgemm.";
#include "avxintrin-emu.h"
#include <stdlib.h>

#if defined(BLOCK_SIZE)
#undef BLOCK_SIZE
#endif
#define BLOCK_SIZE 32
#define SMALL_BLOCK_1 4

#define min(a,b) (((a)<(b))?(a):(b))

static double Ablock[SMALL_BLOCK_1*SMALL_BLOCK_1];
static double Bblock[BLOCK_SIZE*BLOCK_SIZE];

/* This auxiliary subroutine performs a smaller dgemm operation
 *  C := C + A * B
 * where C is M-by-N, A is M-by-K, and B is K-by-N. */
static void do_block (int lda, int M, int N, int K, double* A, double* B, double* C)
{
  int i=0,j=0,k=0;
  double cij0,cij1,cij2,cij3;
  // double* result = (double*) malloc(sizeof(double)*2);
  /* For each row i of A */
  for (j = 0; j < N; ++j) 
  {
    /* For each column j of B */ 
    for (i = 0; i < M/4*4; i += 4)
    {
      /* Compute C(i,j) */
      // __mm256d Cvec = _mm256_loadu_pd(C + (i+j*lda));
      cij0 = C[i+j*lda];
      cij1 = C[i+1+j*lda];
      cij2 = C[i+2+j*lda];
      cij3 = C[i+3+j*lda];
      for (k=0; k < K/4*4; k+=4) {
      	// __mm256d Bvec = _mm256_loadu_pd(B + k+j*lda);
        cij0 += A[i+k*lda] * B[k+j*lda] + A[i+(k+1)*lda] * B[k+1+j*lda] + A[i+(k+2)*lda] * B[k+2+j*lda] + A[i+(k+3)*lda] * B[k+3+j*lda];
        cij1 += A[i+1+(k)*lda] * B[k+j*lda] + A[i+1+(k+1)*lda] * B[k+1+j*lda] + A[i+1+(k+2)*lda] * B[k+2+j*lda] + A[i+1+(k+3)*lda] * B[k+3+j*lda];
        cij2 += A[i+2+(k)*lda] * B[k+j*lda] + A[i+2+(k+1)*lda] * B[k+1+j*lda] + A[i+2+(k+2)*lda] * B[k+2+j*lda] + A[i+2+(k+3)*lda] * B[k+3+j*lda];
        cij3 += A[i+3+(k)*lda] * B[k+j*lda] + A[i+3+(k+1)*lda] * B[k+1+j*lda] + A[i+3+(k+2)*lda] * B[k+2+j*lda] + A[i+3+(k+3)*lda] * B[k+3+j*lda];
      }
      for (k=K/4*4; k < K; k++) {
        cij0 += A[i+k*lda] * B[k+j*lda];
        cij1 += A[i+1+(k)*lda] * B[k+j*lda];
        cij2 += A[i+2+(k)*lda] * B[k+j*lda];
        cij3 += A[i+3+(k)*lda] * B[k+j*lda];
      }
      C[i+j*lda] = cij0;
      C[i+1+j*lda] = cij1;
      C[i+2+j*lda] = cij2;
      C[i+3+j*lda] = cij3;
    }
        /* For each column j of B */ 
    for (i = M/4*4; i < M; i ++)
    {
      /* Compute C(i,j) */
      cij0 = C[i+j*lda];
      for (k=0; k < K/4*4; k+=4) {
        cij0 += A[i+k*lda] * B[k+j*lda] + A[i+(k+1)*lda] * B[k+1+j*lda] + A[i+(k+2)*lda] * B[k+2+j*lda] + A[i+(k+3)*lda] * B[k+3+j*lda];
      }
      for (k=K/4*4; k < K; k++) {
        cij0 += A[i+k*lda] * B[k+j*lda];
      }
      C[i+j*lda] = cij0;
    }
  }
  // free(result);
}

static void transpose(int lda, double *A, double *Atrans) {
  int j = 0;
  for (int i = 0; i < lda; i++) {
    for (j = 0; j < lda/4*4; j+=4) {
      Atrans[j+i*lda] = A[i+j*lda];
      Atrans[(j+1)+i*lda] = A[i+(j+1)*lda];
      Atrans[(j+2)+i*lda] = A[i+(j+2)*lda];
      Atrans[(j+3)+i*lda] = A[i+(j+3)*lda];
    }
    for (; j < lda; j++) {
      Atrans[j+i*lda] = A[i+j*lda];
    }
  }
}


static void smallblock_dgemm (int lda, int M, int N, int K, double* A, double* B, double* C)
{
  /* Accumulate block dgemms into block of C */
  for (int k = 0; k < K; k += SMALL_BLOCK_1)
  {
    /* For each block-row of A */
    for (int i = 0; i < M; i += SMALL_BLOCK_1)
    {
      memcpy(Ablock, A + i + k*lda, sizeof(double)*SMALL_BLOCK_1*SMALL_BLOCK_1);
      /* For each block-column of B */
      for (int j = 0; j < N; j += SMALL_BLOCK_1)
      {
          /* Correct block dimensions if block "goes off edge of" the matrix */
          int M1 = min (SMALL_BLOCK_1, M-i);
          int N1 = min (SMALL_BLOCK_1, N-j);
          int K1 = min (SMALL_BLOCK_1, K-k);

          /* Perform individual block dgemm */
          do_block(lda, M1, N1, K1, Ablock, B + k + j*lda, C + i + j*lda);
      }
    }
  }
}

/* This routine performs a dgemm operation
 *  C := C + A * B
 * where A, B, and C are lda-by-lda matrices stored in column-major format. 
 * On exit, A and B maintain their input values. */  
void square_dgemm (int lda, double* A, double* B, double* C)
{
  // static double *Ablock = malloc(sizeof(double)*SMALL_BLOCK_1*SMALL_BLOCK_1);
//  double *Atrans = malloc(sizeof(double)*lda*lda);
//  transpose(lda, A, Atrans);

  /* For each block-column of B */
  for (int j = 0; j < lda; j += BLOCK_SIZE)
  {
    /* Accumulate block dgemms into block of C */
    for (int k = 0; k < lda; k += BLOCK_SIZE)
    {
      memcpy(Bblock, B + k + j*lda, sizeof(double)*BLOCK_SIZE*BLOCK_SIZE);
      /* For each block-row of A */ 
      for (int i = 0; i < lda; i += BLOCK_SIZE)
      {
          /* Correct block dimensions if block "goes off edge of" the matrix */
          int M = min (BLOCK_SIZE, lda-i);
          int N = min (BLOCK_SIZE, lda-j);
          int K = min (BLOCK_SIZE, lda-k);
          /* Perform individual block dgemm */
          // smallblock_dgemm(lda, M, N, K, A + i + k*lda, B + k + j*lda, C + i + j*lda);
         do_block(lda, M, N, K, A + i + k*lda, Bblock, C + i + j*lda);
      }
    }
  }
}




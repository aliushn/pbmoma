/*
 * Copyright (c) 2002, 2016 Jens Keiner, Stefan Kunis, Daniel Potts
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/**
 * \file polarFFT/mpolar_fft_test.c
 * \brief NFFT-based polar FFT and inverse on  modified polar grid.
 *
 * Computes the NFFT-based polar FFT and its inverse
 * on a modified polar grid for various parameters.
 * \author Markus Fenn
 * \date 2006
 */

#include <math.h>
#include <stdlib.h>
#include <complex.h>

#define NFFT_PRECISION_DOUBLE

#include "nfft3mp.h"

/**
 * \defgroup applications_polarFFT_mpolar mpolar_fft_test
 * \ingroup applications_polarFFT
 * \{
 */

NFFT_R GLOBAL_elapsed_time;

/** Generates the points \f$x_{t,j}\f$ with weights \f$w_{t,j}\f$
 *  for the modified polar grid with \f$T\f$ angles and \f$R\f$ offsets.
 *
 *  We add more concentric circles to the polar grid
 *  and exclude those nodes not located in the unit square, i.e.,
 *  \f[
 *    x_{t,j} := r_j\left(\cos\theta_t, \sin\theta_t\right)^{\top}\,,\qquad
 *    (j,t)^{\top}\in I_{\sqrt{2}R}\times I_T\,.
 *  \f]
 *  with \f$r_j\f$ and \f$\theta_t\f$ as for the polar grid.
 *  The number of nodes for the modified polar grid can be estimated as
 *  \f$M \approx \frac{4}{\pi}\log(1+\sqrt{2}) T R\f$.
 */
static int mpolar_grid(int T, int S, NFFT_R *x, NFFT_R *w)
{
  int t, r;
  NFFT_R W;
  int R2 = 2 * (int)(NFFT_M(lrint)(NFFT_M(ceil)(NFFT_M(sqrt)(NFFT_K(2.0)) * (NFFT_R)(S) / NFFT_K(2.0))));
  NFFT_R xx, yy;
  int M = 0;

  for (t = -T / 2; t < T / 2; t++)
  {
    for (r = -R2 / 2; r < R2 / 2; r++)
    {
      xx = (NFFT_R) (r) / (NFFT_R)(S) * NFFT_M(cos)(NFFT_KPI * (NFFT_R)(t) / (NFFT_R)(T));
      yy = (NFFT_R) (r) / (NFFT_R)(S) * NFFT_M(sin)(NFFT_KPI * (NFFT_R)(t) / (NFFT_R)(T));

      if (((-NFFT_K(0.5) - NFFT_K(1.0) / (NFFT_R) S) <= xx) & (xx <= (NFFT_K(0.5) + NFFT_K(1.0) / (NFFT_R) S))
          & ((-NFFT_K(0.5) - NFFT_K(1.0) / (NFFT_R) S) <= yy)
          & (yy <= (NFFT_K(0.5) + NFFT_K(1.0) / (NFFT_R) S)))
      {
        x[2 * M + 0] = xx;
        x[2 * M + 1] = yy;

        if (r == 0)
          w[M] = NFFT_K(1.0) / NFFT_K(4.0);
        else
          w[M] = NFFT_M(fabs)((NFFT_R) r);

        M++; /** count the knots */
      }
    }
  }

  /** normalize the weights */
  W = NFFT_K(0.0);
  for (t = 0; t < M; t++)
    W += w[t];

  for (t = 0; t < M; t++)
    w[t] /= W;

  return M; /** return the number of knots        */
}

/** discrete mpolar FFT */
static int mpolar_dft(NFFT_C *f_hat, int NN, NFFT_C *f, int T, int S, int m)
{
  double t0, t1;
  int j, k; /**< index for nodes and freqencies   */
  NFFT(plan) my_nfft_plan; /**< plan for the nfft-2D             */

  NFFT_R *x, *w; /**< knots and associated weights     */

  int N[2], n[2];
  int M; /**< number of knots                  */

  N[0] = NN;
  n[0] = 2 * N[0]; /**< oversampling factor sigma=2      */
  N[1] = NN;
  n[1] = 2 * N[1]; /**< oversampling factor sigma=2      */

  x = (NFFT_R *) NFFT(malloc)((size_t)(5 * (T / 2) * S) * (sizeof(NFFT_R)));
  if (x == NULL)
    return EXIT_FAILURE;

  w = (NFFT_R *) NFFT(malloc)((size_t)(5 * (T * S) / 4) * (sizeof(NFFT_R)));
  if (w == NULL)
    return EXIT_FAILURE;

  /** init two dimensional NFFT plan */
  M = mpolar_grid(T, S, x, w);
  NFFT(init_guru)(&my_nfft_plan, 2, N, M, n, m,
      PRE_PHI_HUT | PRE_PSI | MALLOC_X | MALLOC_F_HAT | MALLOC_F | FFTW_INIT
          | FFT_OUT_OF_PLACE,
      FFTW_MEASURE | FFTW_DESTROY_INPUT);

  /** init nodes from mpolar grid*/
  for (j = 0; j < my_nfft_plan.M_total; j++)
  {
    my_nfft_plan.x[2 * j + 0] = x[2 * j + 0];
    my_nfft_plan.x[2 * j + 1] = x[2 * j + 1];
  }

  /** init Fourier coefficients from given image */
  for (k = 0; k < my_nfft_plan.N_total; k++)
    my_nfft_plan.f_hat[k] = f_hat[k];

  t0 = NFFT(clock_gettime_seconds)();

  /** NDFT-2D */
  NFFT(trafo_direct)(&my_nfft_plan);

  t1 = NFFT(clock_gettime_seconds)();
  GLOBAL_elapsed_time = (t1 - t0);

  /** copy result */
  for (j = 0; j < my_nfft_plan.M_total; j++)
    f[j] = my_nfft_plan.f[j];

  /** finalise the plans and free the variables */
  NFFT(finalize)(&my_nfft_plan);
  NFFT(free)(x);
  NFFT(free)(w);

  return EXIT_SUCCESS;
}

/** NFFT-based mpolar FFT */
static int mpolar_fft(NFFT_C *f_hat, int NN, NFFT_C *f, int T, int S, int m)
{
  double t0, t1;
  int j, k; /**< index for nodes and freqencies   */
  NFFT(plan) my_nfft_plan; /**< plan for the nfft-2D             */

  NFFT_R *x, *w; /**< knots and associated weights     */

  int N[2], n[2];
  int M; /**< number of knots                  */

  N[0] = NN;
  n[0] = 2 * N[0]; /**< oversampling factor sigma=2      */
  N[1] = NN;
  n[1] = 2 * N[1]; /**< oversampling factor sigma=2      */

  x = (NFFT_R *) NFFT(malloc)((size_t)(5 * T * S / 2) * (sizeof(NFFT_R)));
  if (x == NULL)
    return EXIT_FAILURE;

  w = (NFFT_R *) NFFT(malloc)((size_t)(5 * T * S / 4) * (sizeof(NFFT_R)));
  if (w == NULL)
    return EXIT_FAILURE;

  /** init two dimensional NFFT plan */
  M = mpolar_grid(T, S, x, w);
  NFFT(init_guru)(&my_nfft_plan, 2, N, M, n, m,
      PRE_PHI_HUT | PRE_PSI | MALLOC_X | MALLOC_F_HAT | MALLOC_F | FFTW_INIT
          | FFT_OUT_OF_PLACE,
      FFTW_MEASURE | FFTW_DESTROY_INPUT);

  /** init nodes from mpolar grid*/

  for (j = 0; j < my_nfft_plan.M_total; j++)
  {
    my_nfft_plan.x[2 * j + 0] = x[2 * j + 0];
    my_nfft_plan.x[2 * j + 1] = x[2 * j + 1];
  }

  /** precompute psi, the entries of the matrix B */
  if (my_nfft_plan.flags & PRE_LIN_PSI)
    NFFT(precompute_lin_psi)(&my_nfft_plan);

  if (my_nfft_plan.flags & PRE_PSI)
    NFFT(precompute_psi)(&my_nfft_plan);

  if (my_nfft_plan.flags & PRE_FULL_PSI)
    NFFT(precompute_full_psi)(&my_nfft_plan);

  /** init Fourier coefficients from given image */
  for (k = 0; k < my_nfft_plan.N_total; k++)
    my_nfft_plan.f_hat[k] = f_hat[k];

  t0 = NFFT(clock_gettime_seconds)();

  /** NFFT-2D */
  NFFT(trafo)(&my_nfft_plan);

  t1 = NFFT(clock_gettime_seconds)();
  GLOBAL_elapsed_time = (t1 - t0);

  /** copy result */
  for (j = 0; j < my_nfft_plan.M_total; j++)
    f[j] = my_nfft_plan.f[j];

  /** finalise the plans and free the variables */
  NFFT(finalize)(&my_nfft_plan);
  NFFT(free)(x);
  NFFT(free)(w);

  return EXIT_SUCCESS;
}

/** inverse NFFT-based mpolar FFT */
static int inverse_mpolar_fft(NFFT_C *f, int T, int S, NFFT_C *f_hat, int NN, int max_i,
    int m)
{
  double t0, t1;
  int j, k; /**< index for nodes and freqencies   */
  NFFT(plan) my_nfft_plan; /**< plan for the nfft-2D             */
  SOLVER(plan_complex) my_infft_plan; /**< plan for the inverse nfft        */

  NFFT_R *x, *w; /**< knots and associated weights     */
  int l; /**< index for iterations             */

  int N[2], n[2];
  int M; /**< number of knots                  */

  N[0] = NN;
  n[0] = 2 * N[0]; /**< oversampling factor sigma=2      */
  N[1] = NN;
  n[1] = 2 * N[1]; /**< oversampling factor sigma=2      */

  x = (NFFT_R *) NFFT(malloc)((size_t)(5 * T * S / 2) * (sizeof(NFFT_R)));
  if (x == NULL)
    return EXIT_FAILURE;

  w = (NFFT_R *) NFFT(malloc)((size_t)(5 * T * S / 4) * (sizeof(NFFT_R)));
  if (w == NULL)
    return EXIT_FAILURE;

  /** init two dimensional NFFT plan */
  M = mpolar_grid(T, S, x, w);
  NFFT(init_guru)(&my_nfft_plan, 2, N, M, n, m,
      PRE_PHI_HUT | PRE_PSI | MALLOC_X | MALLOC_F_HAT | MALLOC_F | FFTW_INIT
          | FFT_OUT_OF_PLACE,
      FFTW_MEASURE | FFTW_DESTROY_INPUT);

  /** init two dimensional infft plan */
  SOLVER(init_advanced_complex)(&my_infft_plan,
      (NFFT(mv_plan_complex)*) (&my_nfft_plan), CGNR | PRECOMPUTE_WEIGHT);

  /** init nodes, given samples and weights */
  for (j = 0; j < my_nfft_plan.M_total; j++)
  {
    my_nfft_plan.x[2 * j + 0] = x[2 * j + 0];
    my_nfft_plan.x[2 * j + 1] = x[2 * j + 1];
    my_infft_plan.y[j] = f[j];
    my_infft_plan.w[j] = w[j];
  }

  /** precompute psi, the entries of the matrix B */
  if (my_nfft_plan.flags & PRE_LIN_PSI)
    NFFT(precompute_lin_psi)(&my_nfft_plan);

  if (my_nfft_plan.flags & PRE_PSI)
    NFFT(precompute_psi)(&my_nfft_plan);

  if (my_nfft_plan.flags & PRE_FULL_PSI)
    NFFT(precompute_full_psi)(&my_nfft_plan);

  /** initialise damping factors */
  if (my_infft_plan.flags & PRECOMPUTE_DAMP)
    for (j = 0; j < my_nfft_plan.N[0]; j++)
      for (k = 0; k < my_nfft_plan.N[1]; k++)
      {
        my_infft_plan.w_hat[j * my_nfft_plan.N[1] + k] = (
            NFFT_M(sqrt)(
                NFFT_M(pow)((NFFT_R)(j - my_nfft_plan.N[0] / 2), NFFT_K(2.0))
                    + NFFT_M(pow)((NFFT_R)(k - my_nfft_plan.N[1] / 2), NFFT_K(2.0)))
                > (NFFT_R)(my_nfft_plan.N[0] / 2) ? NFFT_K(0.0) : NFFT_K(1.0));
      }

  /** initialise some guess f_hat_0 */
  for (k = 0; k < my_nfft_plan.N_total; k++)
    my_infft_plan.f_hat_iter[k] = NFFT_K(0.0) + _Complex_I * NFFT_K(0.0);

  t0 = NFFT(clock_gettime_seconds)();

  /** solve the system */
  SOLVER(before_loop_complex)(&my_infft_plan);

  if (max_i < 1)
  {
    l = 1;
    for (k = 0; k < my_nfft_plan.N_total; k++)
      my_infft_plan.f_hat_iter[k] = my_infft_plan.p_hat_iter[k];
  }
  else
  {
    for (l = 1; l <= max_i; l++)
    {
      SOLVER(loop_one_step_complex)(&my_infft_plan);
    }
  }

  t1 = NFFT(clock_gettime_seconds)();
  GLOBAL_elapsed_time = (t1 - t0);

  /** copy result */
  for (k = 0; k < my_nfft_plan.N_total; k++)
    f_hat[k] = my_infft_plan.f_hat_iter[k];

  /** finalise the plans and free the variables */
  SOLVER(finalize_complex)(&my_infft_plan);
  NFFT(finalize)(&my_nfft_plan);
  NFFT(free)(x);
  NFFT(free)(w);

  return EXIT_SUCCESS;
}

/** Comparison of the FFTW, mpolar FFT, and inverse mpolar FFT */
static int comparison_fft(FILE *fp, int N, int T, int S)
{
  double t0, t1;
  FFTW(plan) my_fftw_plan;
  NFFT_C *f_hat, *f;
  int m, k;
  NFFT_R t_fft, t_dft_mpolar;

  f_hat = (NFFT_C *) NFFT(malloc)(sizeof(NFFT_C) * (size_t)(N * N));
  f = (NFFT_C *) NFFT(malloc)(sizeof(NFFT_C) * (size_t)((T * S / 4) * 5));

  my_fftw_plan = FFTW(plan_dft_2d)(N, N, f_hat, f, FFTW_BACKWARD, FFTW_MEASURE);

  for (k = 0; k < N * N; k++)
    f_hat[k] = NFFT(drand48)() + _Complex_I * NFFT(drand48)();

  t0 = NFFT(clock_gettime_seconds)();
  for (m = 0; m < 65536 / N; m++)
  {
    FFTW(execute)(my_fftw_plan);
    /* touch */
    f_hat[2] = NFFT_K(2.0) * f_hat[0];
  }
  t1 = NFFT(clock_gettime_seconds)();
  GLOBAL_elapsed_time = (t1 - t0);
  t_fft = (NFFT_R)(N) * GLOBAL_elapsed_time / NFFT_K(65536.0);

  if (N < 256)
  {
    mpolar_dft(f_hat, N, f, T, S, 1);
    t_dft_mpolar = GLOBAL_elapsed_time;
  }

  for (m = 3; m <= 9; m += 3)
  {
    if ((m == 3) && (N < 256))
      fprintf(fp, "%d\t&\t&\t%1.1" NFFT__FES__ "&\t%1.1" NFFT__FES__ "&\t%d\t", N, t_fft, t_dft_mpolar, m);
    else if (m == 3)
      fprintf(fp, "%d\t&\t&\t%1.1" NFFT__FES__ "&\t       &\t%d\t", N, t_fft, m);
    else
      fprintf(fp, "  \t&\t&\t       &\t       &\t%d\t", m);

    printf("N=%d\tt_fft=%1.1" NFFT__FES__ "\tt_dft_mpolar=%1.1" NFFT__FES__ "\tm=%d\t", N, t_fft,
        t_dft_mpolar, m);

    mpolar_fft(f_hat, N, f, T, S, m);
    fprintf(fp, "%1.1" NFFT__FES__ "&\t", GLOBAL_elapsed_time);
    printf("t_mpolar=%1.1" NFFT__FES__ "\t", GLOBAL_elapsed_time);
    inverse_mpolar_fft(f, T, S, f_hat, N, 2 * m, m);
    if (m == 9)
      fprintf(fp, "%1.1" NFFT__FES__ "\\\\\\hline\n", GLOBAL_elapsed_time);
    else
      fprintf(fp, "%1.1" NFFT__FES__ "\\\\\n", GLOBAL_elapsed_time);
    printf("t_impolar=%1.1" NFFT__FES__ "\n", GLOBAL_elapsed_time);
  }

  fflush(fp);

  NFFT(free)(f);
  NFFT(free)(f_hat);

  return EXIT_SUCCESS;
}

/** test program for various parameters */
int main(int argc, char **argv)
{
  int N; /**< mpolar FFT size NxN              */
  int T, S; /**< number of directions/offsets     */
  int M; /**< number of knots of mpolar grid   */
  NFFT_R *x, *w; /**< knots and associated weights     */
  NFFT_C *f_hat, *f, *f_direct, *f_tilde;
  int k;
  int max_i; /**< number of iterations             */
  int m;
  NFFT_R temp1, temp2, E_max = NFFT_K(0.0);
  FILE *fp1, *fp2;
  char filename[30];
  int logN;

  if (argc != 4)
  {
    printf("mpolar_fft_test N T R \n");
    printf("\n");
    printf("N          mpolar FFT of size NxN    \n");
    printf("T          number of slopes          \n");
    printf("R          number of offsets         \n");

    /** Hence, comparison of the FFTW, mpolar FFT, and inverse mpolar FFT */
    printf("\nHence, comparison FFTW, mpolar FFT and inverse mpolar FFT\n");
    fp1 = fopen("mpolar_comparison_fft.dat", "w");
    if (fp1 == NULL)
      return (-1);
    for (logN = 4; logN <= 8; logN++)
      comparison_fft(fp1, (int)(1U << logN), 3 * (int)(1U << logN),
          3 * (int)(1U << (logN - 1)));
    fclose(fp1);

    exit(EXIT_FAILURE);
  }

  N = atoi(argv[1]);
  T = atoi(argv[2]);
  S = atoi(argv[3]);
  printf("N=%d, modified polar grid with T=%d, R=%d => ", N, T, S);

  x = (NFFT_R *) NFFT(malloc)((size_t)(5 * T * S / 2) * (sizeof(NFFT_R)));
  w = (NFFT_R *) NFFT(malloc)((size_t)(5 * T * S / 4) * (sizeof(NFFT_R)));

  f_hat = (NFFT_C *) NFFT(malloc)(sizeof(NFFT_C) * (size_t)(N * N));
  f = (NFFT_C *) NFFT(malloc)(sizeof(NFFT_C) * (size_t)(1.25 * T * S)); /* 4/pi*log(1+sqrt(2)) = 1.122... < 1.25 */
  f_direct = (NFFT_C *) NFFT(malloc)(sizeof(NFFT_C) * (size_t)(1.25 * T * S));
  f_tilde = (NFFT_C *) NFFT(malloc)(sizeof(NFFT_C) * (size_t)(N * N));

  /** generate knots of mpolar grid */
  M = mpolar_grid(T, S, x, w);
  printf("M=%d.\n", M);

  /** load data */
  fp1 = fopen("input_data_r.dat", "r");
  fp2 = fopen("input_data_i.dat", "r");
  if ((fp1 == NULL) || (fp2 == NULL))
    return (-1);
  for (k = 0; k < N * N; k++)
  {
    fscanf(fp1, NFFT__FR__ " ", &temp1);
    fscanf(fp2, NFFT__FR__ " ", &temp2);
    f_hat[k] = temp1 + _Complex_I * temp2;
  }
  fclose(fp1);
  fclose(fp2);

  /** direct mpolar FFT */
  mpolar_dft(f_hat, N, f_direct, T, S, 1);
  //  mpolar_fft(f_hat,N,f_direct,T,R,12);

  /** Test of the mpolar FFT with different m */
  printf("\nTest of the mpolar FFT: \n");
  fp1 = fopen("mpolar_fft_error.dat", "w+");
  for (m = 1; m <= 12; m++)
  {
    /** fast mpolar FFT */
    mpolar_fft(f_hat, N, f, T, S, m);

    /** compute error of fast mpolar FFT */
    E_max = NFFT(error_l_infty_complex)(f_direct, f, M);
    printf("m=%2d: E_max = %" NFFT__FES__ "\n", m, E_max);
    fprintf(fp1, "%" NFFT__FES__ "\n", E_max);
  }
  fclose(fp1);

  /** Test of the inverse mpolar FFT for different m in dependece of the iteration number*/
  for (m = 3; m <= 9; m += 3)
  {
    printf("\nTest of the inverse mpolar FFT for m=%d: \n", m);
    sprintf(filename, "mpolar_ifft_error%d.dat", m);
    fp1 = fopen(filename, "w+");
    for (max_i = 0; max_i <= 20; max_i += 2)
    {
      /** inverse mpolar FFT */
      inverse_mpolar_fft(f_direct, T, S, f_tilde, N, max_i, m);

      /** compute maximum relativ error */
      E_max = NFFT(error_l_infty_complex)(f_hat, f_tilde, N * N);
      printf("%3d iterations: E_max = %" NFFT__FES__ "\n", max_i, E_max);
      fprintf(fp1, "%" NFFT__FES__ "\n", E_max);
    }
    fclose(fp1);
  }

  /** free the variables */
  NFFT(free)(x);
  NFFT(free)(w);
  NFFT(free)(f_hat);
  NFFT(free)(f);
  NFFT(free)(f_direct);
  NFFT(free)(f_tilde);

  return EXIT_SUCCESS;
}
/* \} */

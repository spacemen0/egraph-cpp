#pragma once

#ifdef __APPLE__

#include <Accelerate/Accelerate.h>

#define LAPACK_COL_MAJOR 102

namespace egraph {
inline int LAPACKE_dtrtri(int matrix_layout, char uplo, char diag, int n, double *a, int lda) {
    if (matrix_layout != LAPACK_COL_MAJOR)
        return -1;
    int info = 0;
    dtrtri_(&uplo, &diag, &n, a, &lda, &info);
    return info;
}

inline int LAPACKE_dorgqr(int matrix_layout, int m, int n, int k, double *a, int lda, const double *tau) {
    if (matrix_layout != LAPACK_COL_MAJOR)
        return -1;
    int info = 0;

    int lwork = -1;
    double wkopt = 0.0;
    // workspace query
    dorgqr_(&m, &n, &k, a, &lda, const_cast<double *>(tau), &wkopt, &lwork, &info);
    lwork = (int)wkopt;
    double *work = new double[lwork];
    dorgqr_(&m, &n, &k, a, &lda, const_cast<double *>(tau), work, &lwork, &info);
    delete[] work;
    return info;
}

inline int LAPACKE_dormqr(
    int matrix_layout, char side, char trans, int m, int n, int k, const double *a, int lda, const double *tau,
    double *c, int ldc) {
    if (matrix_layout != LAPACK_COL_MAJOR)
        return -1;
    int info = 0;

    int lwork = -1;
    double wkopt = 0.0;
    // workspace query
    dormqr_(
        &side, &trans, &m, &n, &k, const_cast<double *>(a), &lda, const_cast<double *>(tau), c, &ldc, &wkopt, &lwork,
        &info);
    lwork = (int)wkopt;
    double *work = new double[lwork];
    dormqr_(
        &side, &trans, &m, &n, &k, const_cast<double *>(a), &lda, const_cast<double *>(tau), c, &ldc, work, &lwork,
        &info);
    delete[] work;
    return info;
}

inline int LAPACKE_dpotrf(int matrix_layout, char uplo, int n, double *a, int lda) {
    if (matrix_layout != LAPACK_COL_MAJOR)
        return -1;
    int info = 0;
    dpotrf_(&uplo, &n, a, &lda, &info);
    return info;
}

inline int LAPACKE_dgeqrf(int matrix_layout, int m, int n, double *a, int lda, double *tau) {
    if (matrix_layout != LAPACK_COL_MAJOR)
        return -1;
    int info = 0;

    int lwork = -1;
    double wkopt = 0.0;
    // workspace query
    dgeqrf_(&m, &n, a, &lda, tau, &wkopt, &lwork, &info);
    lwork = (int)wkopt;
    double *work = new double[lwork];
    dgeqrf_(&m, &n, a, &lda, tau, work, &lwork, &info);
    delete[] work;
    return info;
}
#endif

} // namespace egraph

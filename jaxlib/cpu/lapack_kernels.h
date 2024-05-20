/* Copyright 2021 The JAX Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef JAXLIB_CPU_LAPACK_KERNELS_H_
#define JAXLIB_CPU_LAPACK_KERNELS_H_

#include <cstdint>
#include <optional>

#include "xla/ffi/api/ffi.h"

// Underlying function pointers (e.g., KERNEL_CLASS::Fn) are initialized either
// by the pybind wrapper that links them to an existing SciPy lapack instance,
// or using the lapack_kernels_strong.cc static initialization to link them
// directly to lapack for use in a pure C++ context.

namespace jax {

struct MatrixParams {
  enum class Side : char { kLeft = 'L', kRight = 'R' };
  enum class UpLo : char { kLower = 'L', kUpper = 'U' };
  enum class Diag : char { kNonUnit = 'N', kUnit = 'U' };
  enum class Transpose : char {
    kNoTrans = 'N',
    kTrans = 'T',
    kConjTrans = 'C'
  };
};

namespace svd {

enum class ComputationMode : char {
  kComputeFullUVt = 'A',  // Compute U and VT
  kComputeMinUVt = 'S',   // Compute min(M, N) columns of U and rows of VT
  kComputeVtOverwriteXPartialU = 'O',  // Compute VT, overwrite X
                                       // with partial U
  kNoComputeUVt = 'N',                 // Do not compute U or VT
};

inline bool ComputesUV(ComputationMode mode) {
  return mode == ComputationMode::kComputeFullUVt ||
         mode == ComputationMode::kComputeMinUVt;
}

}  // namespace svd

namespace eig {

enum class ComputationMode : char {
  kNoEigenvectors = 'N',
  kComputeEigenvectors = 'V',
};

}

namespace schur {

enum class ComputationMode : char {
  kNoComputeSchurVectors = 'N',
  kComputeSchurVectors = 'V',
};

enum class Sort : char { kNoSortEigenvalues = 'N', kSortEigenvalues = 'S' };

}  // namespace schur

template <typename KernelType>
void AssignKernelFn(void* func) {
  KernelType::fn = reinterpret_cast<typename KernelType::FnType*>(func);
}

}  // namespace jax

#define DEFINE_CHAR_ENUM_ATTR_DECODING(ATTR)                             \
  template <>                                                            \
  struct xla::ffi::AttrDecoding<ATTR> {                                  \
    using Type = ATTR;                                                   \
    static std::optional<Type> Decode(XLA_FFI_AttrType type, void* attr, \
                                      DiagnosticEngine& diagnostic);     \
  }

// XLA needs attributes to have deserialization method specified
DEFINE_CHAR_ENUM_ATTR_DECODING(jax::MatrixParams::Side);
DEFINE_CHAR_ENUM_ATTR_DECODING(jax::MatrixParams::UpLo);
DEFINE_CHAR_ENUM_ATTR_DECODING(jax::MatrixParams::Transpose);
DEFINE_CHAR_ENUM_ATTR_DECODING(jax::MatrixParams::Diag);
DEFINE_CHAR_ENUM_ATTR_DECODING(jax::svd::ComputationMode);
DEFINE_CHAR_ENUM_ATTR_DECODING(jax::eig::ComputationMode);
DEFINE_CHAR_ENUM_ATTR_DECODING(jax::schur::ComputationMode);
DEFINE_CHAR_ENUM_ATTR_DECODING(jax::schur::Sort);

#undef DEFINE_CHAR_ENUM_ATTR_DECODING

namespace jax {

using lapack_int = int;
inline constexpr auto LapackIntDtype = ::xla::ffi::DataType::S32;
static_assert(
    std::is_same_v<::xla::ffi::NativeType<LapackIntDtype>, lapack_int>);

// lapack trsm

template <::xla::ffi::DataType dtype>
struct TriMatrixEquationSolver {
  using ValueType = ::xla::ffi::NativeType<dtype>;
  using FnType = void(char* side, char* uplo, char* transa, char* diag,
                      lapack_int* m, lapack_int* n, ValueType* alpha,
                      ValueType* a, lapack_int* lda, ValueType* b,
                      lapack_int* ldb);

  inline static FnType* fn = nullptr;
  static ::xla::ffi::Error Kernel(
      ::xla::ffi::Buffer<dtype> x, ::xla::ffi::Buffer<dtype> y,
      ::xla::ffi::BufferR0<dtype> alpha, ::xla::ffi::ResultBuffer<dtype> y_out,
      MatrixParams::Side side, MatrixParams::UpLo uplo,
      MatrixParams::Transpose trans_x, MatrixParams::Diag diag);
};

// lapack getrf

template <::xla::ffi::DataType dtype>
struct LuDecomposition {
  using ValueType = ::xla::ffi::NativeType<dtype>;
  using FnType = void(lapack_int* m, lapack_int* n, ValueType* a,
                      lapack_int* lda, lapack_int* ipiv, lapack_int* info);

  inline static FnType* fn = nullptr;
  static ::xla::ffi::Error Kernel(
      ::xla::ffi::Buffer<dtype> x, ::xla::ffi::ResultBuffer<dtype> x_out,
      ::xla::ffi::ResultBuffer<LapackIntDtype> ipiv,
      ::xla::ffi::ResultBuffer<LapackIntDtype> info);
};

// lapack geqrf

template <::xla::ffi::DataType dtype>
struct QrFactorization {
  using ValueType = ::xla::ffi::NativeType<dtype>;
  using FnType = void(lapack_int* m, lapack_int* n, ValueType* a,
                      lapack_int* lda, ValueType* tau, ValueType* work,
                      lapack_int* lwork, lapack_int* info);

  inline static FnType* fn = nullptr;

  static ::xla::ffi::Error Kernel(::xla::ffi::Buffer<dtype> x,
                                  ::xla::ffi::ResultBuffer<dtype> x_out,
                                  ::xla::ffi::ResultBuffer<dtype> tau,
                                  ::xla::ffi::ResultBuffer<LapackIntDtype> info,
                                  ::xla::ffi::ResultBuffer<dtype> work);

  static int64_t GetWorkspaceSize(lapack_int x_rows, lapack_int x_cols);
};

// lapack orgqr

template <::xla::ffi::DataType dtype>
struct OrthogonalQr {
  using ValueType = ::xla::ffi::NativeType<dtype>;
  using FnType = void(lapack_int* m, lapack_int* n, lapack_int* k, ValueType* a,
                      lapack_int* lda, ValueType* tau, ValueType* work,
                      lapack_int* lwork, lapack_int* info);

  inline static FnType* fn = nullptr;

  static ::xla::ffi::Error Kernel(::xla::ffi::Buffer<dtype> x,
                                  ::xla::ffi::Buffer<dtype> tau,
                                  ::xla::ffi::ResultBuffer<dtype> x_out,
                                  ::xla::ffi::ResultBuffer<LapackIntDtype> info,
                                  ::xla::ffi::ResultBuffer<dtype> work);

  static int64_t GetWorkspaceSize(lapack_int x_rows, lapack_int x_cols,
                                  lapack_int tau_size);
};

// lapack potrf

template <::xla::ffi::DataType dtype>
struct CholeskyFactorization {
  using ValueType = ::xla::ffi::NativeType<dtype>;
  using FnType = void(char* uplo, lapack_int* n, ValueType* a, lapack_int* lda,
                      lapack_int* info);

  inline static FnType* fn = nullptr;

  static ::xla::ffi::Error Kernel(
      ::xla::ffi::Buffer<dtype> x, MatrixParams::UpLo uplo,
      ::xla::ffi::ResultBuffer<dtype> x_out,
      ::xla::ffi::ResultBuffer<LapackIntDtype> info);
};

// lapack gesdd

template <::xla::ffi::DataType dtype>
struct SingularValueDecomposition {
  static_assert(!::xla::ffi::IsComplexType<dtype>(),
                "There exists a separate implementation for Complex types");
  using ValueType = ::xla::ffi::NativeType<dtype>;
  using FnType = void(char* jobz, lapack_int* m, lapack_int* n, ValueType* a,
                      lapack_int* lda, ValueType* s, ValueType* u,
                      lapack_int* ldu, ValueType* vt, lapack_int* ldvt,
                      ValueType* work, lapack_int* lwork, lapack_int* iwork,
                      lapack_int* info);

  inline static FnType* fn = nullptr;

  static ::xla::ffi::Error Kernel(
      ::xla::ffi::Buffer<dtype> x, ::xla::ffi::ResultBuffer<dtype> x_out,
      ::xla::ffi::ResultBuffer<dtype> singular_values,
      ::xla::ffi::ResultBuffer<dtype> u, ::xla::ffi::ResultBuffer<dtype> vt,
      ::xla::ffi::ResultBuffer<LapackIntDtype> info,
      ::xla::ffi::ResultBuffer<LapackIntDtype> iwork,
      ::xla::ffi::ResultBuffer<dtype> work, svd::ComputationMode mode);

  static int64_t GetWorkspaceSize(lapack_int x_rows, lapack_int x_cols,
                                  svd::ComputationMode mode);
};

template <::xla::ffi::DataType dtype>
struct SingularValueDecompositionComplex {
  static_assert(::xla::ffi::IsComplexType<dtype>());

  using ValueType = ::xla::ffi::NativeType<dtype>;
  using RealType = ::xla::ffi::NativeType<::xla::ffi::ToReal(dtype)>;
  using FnType = void(char* jobz, lapack_int* m, lapack_int* n, ValueType* a,
                      lapack_int* lda, RealType* s, ValueType* u,
                      lapack_int* ldu, ValueType* vt, lapack_int* ldvt,
                      ValueType* work, lapack_int* lwork, RealType* rwork,
                      lapack_int* iwork, lapack_int* info);

  inline static FnType* fn = nullptr;

  static ::xla::ffi::Error Kernel(
      ::xla::ffi::Buffer<dtype> x, ::xla::ffi::ResultBuffer<dtype> x_out,
      ::xla::ffi::ResultBuffer<::xla::ffi::ToReal(dtype)> singular_values,
      ::xla::ffi::ResultBuffer<dtype> u, ::xla::ffi::ResultBuffer<dtype> vt,
      ::xla::ffi::ResultBuffer<LapackIntDtype> info,
      ::xla::ffi::ResultBuffer<::xla::ffi::ToReal(dtype)> rwork,
      ::xla::ffi::ResultBuffer<LapackIntDtype> iwork,
      ::xla::ffi::ResultBuffer<dtype> work, svd::ComputationMode mode);

  static int64_t GetWorkspaceSize(lapack_int x_rows, lapack_int x_cols,
                                  svd::ComputationMode mode);
};

namespace svd {

template <::xla::ffi::DataType dtype>
using SVDType = std::conditional_t<::xla::ffi::IsComplexType<dtype>(),
                                   SingularValueDecompositionComplex<dtype>,
                                   SingularValueDecomposition<dtype>>;

lapack_int GetIntWorkspaceSize(int64_t x_rows, int64_t x_cols);
lapack_int GetRealWorkspaceSize(int64_t x_rows, int64_t x_cols,
                                ComputationMode mode);

}  // namespace svd

// lapack syevd/heevd

namespace eig {

// Eigenvalue Decomposition
lapack_int GetWorkspaceSize(int64_t x_cols, ComputationMode mode);
lapack_int GetIntWorkspaceSize(int64_t x_cols, ComputationMode mode);

// Hermitian Eigenvalue Decomposition
lapack_int GetComplexWorkspaceSize(int64_t x_cols, ComputationMode mode);
lapack_int GetRealWorkspaceSize(int64_t x_cols, ComputationMode mode);

}  // namespace eig

template <::xla::ffi::DataType dtype>
struct EigenvalueDecompositionSymmetric {
  static_assert(!::xla::ffi::IsComplexType<dtype>(),
                "There exists a separate implementation for Complex types");

  using ValueType = ::xla::ffi::NativeType<dtype>;
  using FnType = void(char* jobz, char* uplo, lapack_int* n, ValueType* a,
                      lapack_int* lda, ValueType* w, ValueType* work,
                      lapack_int* lwork, lapack_int* iwork, lapack_int* liwork,
                      lapack_int* info);

  inline static FnType* fn = nullptr;

  static ::xla::ffi::Error Kernel(
      ::xla::ffi::Buffer<dtype> x, MatrixParams::UpLo uplo,
      ::xla::ffi::ResultBuffer<dtype> x_out,
      ::xla::ffi::ResultBuffer<dtype> eigenvalues,
      ::xla::ffi::ResultBuffer<LapackIntDtype> info,
      ::xla::ffi::ResultBuffer<dtype> work,
      ::xla::ffi::ResultBuffer<LapackIntDtype> iwork,
      eig::ComputationMode mode);
};

template <::xla::ffi::DataType dtype>
struct EigenvalueDecompositionHermitian {
  static_assert(::xla::ffi::IsComplexType<dtype>());

  using ValueType = ::xla::ffi::NativeType<dtype>;
  using RealType = ::xla::ffi::NativeType<::xla::ffi::ToReal(dtype)>;
  using FnType = void(char* jobz, char* uplo, lapack_int* n, ValueType* a,
                      lapack_int* lda, RealType* w, ValueType* work,
                      lapack_int* lwork, RealType* rwork, lapack_int* lrwork,
                      lapack_int* iwork, lapack_int* liwork, lapack_int* info);

  inline static FnType* fn = nullptr;

  static ::xla::ffi::Error Kernel(
      ::xla::ffi::Buffer<dtype> x, MatrixParams::UpLo uplo,
      ::xla::ffi::ResultBuffer<dtype> x_out,
      ::xla::ffi::ResultBuffer<::xla::ffi::ToReal(dtype)> eigenvalues,
      ::xla::ffi::ResultBuffer<LapackIntDtype> info,
      ::xla::ffi::ResultBuffer<dtype> work,
      ::xla::ffi::ResultBuffer<::xla::ffi::ToReal(dtype)> rwork,
      ::xla::ffi::ResultBuffer<LapackIntDtype> iwork,
      eig::ComputationMode mode);
};

// lapack geev

template <::xla::ffi::DataType dtype>
struct EigenvalueDecomposition {
  static_assert(!::xla::ffi::IsComplexType<dtype>(),
                "There exists a separate implementation for Complex types");

  using ValueType = ::xla::ffi::NativeType<dtype>;
  using FnType = void(char* jobvl, char* jobvr, lapack_int* n, ValueType* a,
                      lapack_int* lda, ValueType* wr, ValueType* wi,
                      ValueType* vl, lapack_int* ldvl, ValueType* vr,
                      lapack_int* ldvr, ValueType* work, lapack_int* lwork,
                      lapack_int* info);

  inline static FnType* fn = nullptr;

  static ::xla::ffi::Error Kernel(
      ::xla::ffi::Buffer<dtype> x, eig::ComputationMode compute_left,
      eig::ComputationMode compute_right,
      ::xla::ffi::ResultBuffer<dtype> eigvals_real,
      ::xla::ffi::ResultBuffer<dtype> eigvals_imag,
      ::xla::ffi::ResultBuffer<::xla::ffi::ToComplex(dtype)> eigvecs_left,
      ::xla::ffi::ResultBuffer<::xla::ffi::ToComplex(dtype)> eigvecs_right,
      ::xla::ffi::ResultBuffer<LapackIntDtype> info,
      ::xla::ffi::ResultBuffer<dtype> x_work,
      ::xla::ffi::ResultBuffer<::xla::ffi::ToReal(dtype)> work_eigvecs_left,
      ::xla::ffi::ResultBuffer<::xla::ffi::ToReal(dtype)> work_eigvecs_right);

  static int64_t GetWorkspaceSize(lapack_int x_cols,
                                  eig::ComputationMode compute_left,
                                  eig::ComputationMode compute_right);
};

template <::xla::ffi::DataType dtype>
struct EigenvalueDecompositionComplex {
  static_assert(::xla::ffi::IsComplexType<dtype>());

  using ValueType = ::xla::ffi::NativeType<dtype>;
  using RealType = ::xla::ffi::NativeType<::xla::ffi::ToReal(dtype)>;
  using FnType = void(char* jobvl, char* jobvr, lapack_int* n, ValueType* a,
                      lapack_int* lda, ValueType* w, ValueType* vl,
                      lapack_int* ldvl, ValueType* vr, lapack_int* ldvr,
                      ValueType* work, lapack_int* lwork, RealType* rwork,
                      lapack_int* info);

  inline static FnType* fn = nullptr;

  static ::xla::ffi::Error Kernel(
      ::xla::ffi::Buffer<dtype> x, eig::ComputationMode compute_left,
      eig::ComputationMode compute_right,
      ::xla::ffi::ResultBuffer<dtype> eigvals,
      ::xla::ffi::ResultBuffer<dtype> eigvecs_left,
      ::xla::ffi::ResultBuffer<dtype> eigvecs_right,
      ::xla::ffi::ResultBuffer<LapackIntDtype> info,
      ::xla::ffi::ResultBuffer<dtype> x_work,
      ::xla::ffi::ResultBuffer<::xla::ffi::ToReal(dtype)> rwork);

  static int64_t GetWorkspaceSize(lapack_int x_cols,
                                  eig::ComputationMode compute_left,
                                  eig::ComputationMode compute_right);
};

// lapack gees

template <::xla::ffi::DataType dtype>
struct SchurDecomposition {
  using ValueType = ::xla::ffi::NativeType<dtype>;
  using FnType = void(char* jobvs, char* sort,
                      bool (*select)(ValueType, ValueType), lapack_int* n,
                      ValueType* a, lapack_int* lda, lapack_int* sdim,
                      ValueType* wr, ValueType* wi, ValueType* vs,
                      lapack_int* ldvs, ValueType* work, lapack_int* lwork,
                      bool* bwork, lapack_int* info);

  inline static FnType* fn = nullptr;

  static ::xla::ffi::Error Kernel(
      ::xla::ffi::Buffer<dtype> x, schur::ComputationMode mode,
      schur::Sort sort, ::xla::ffi::ResultBuffer<dtype> x_out,
      ::xla::ffi::ResultBuffer<dtype> eigvals_real,
      ::xla::ffi::ResultBuffer<dtype> eigvals_imag,
      ::xla::ffi::ResultBuffer<dtype> schur_vectors,
      ::xla::ffi::ResultBuffer<LapackIntDtype> selected_eigvals,
      ::xla::ffi::ResultBuffer<LapackIntDtype> info);

  static int64_t GetWorkspaceSize(lapack_int x_cols,
                                  schur::ComputationMode mode,
                                  schur::Sort sort);
};

template <::xla::ffi::DataType dtype>
struct SchurDecompositionComplex {
  using ValueType = ::xla::ffi::NativeType<dtype>;
  using RealType = ::xla::ffi::NativeType<::xla::ffi::ToReal(dtype)>;
  using FnType = void(char* jobvs, char* sort, bool (*select)(ValueType),
                      lapack_int* n, ValueType* a, lapack_int* lda,
                      lapack_int* sdim, ValueType* w, ValueType* vs,
                      lapack_int* ldvs, ValueType* work, lapack_int* lwork,
                      RealType* rwork, bool* bwork, lapack_int* info);

  inline static FnType* fn = nullptr;

  static ::xla::ffi::Error Kernel(
      ::xla::ffi::Buffer<dtype> x, schur::ComputationMode mode,
      schur::Sort sort, ::xla::ffi::ResultBuffer<dtype> x_out,
      ::xla::ffi::ResultBuffer<dtype> eigvals,
      ::xla::ffi::ResultBuffer<dtype> schur_vectors,
      ::xla::ffi::ResultBuffer<LapackIntDtype> selected_eigvals,
      ::xla::ffi::ResultBuffer<LapackIntDtype> info,
      ::xla::ffi::ResultBuffer<::xla::ffi::ToReal(dtype)> rwork);

  static int64_t GetWorkspaceSize(lapack_int x_cols,
                                  schur::ComputationMode mode,
                                  schur::Sort sort);
};

// lapack gehrd

// Reduces a non-symmetric square matrix to upper Hessenberg form.
template <::xla::ffi::DataType dtype>
struct HessenbergDecomposition {
  using ValueType = ::xla::ffi::NativeType<dtype>;
  using FnType = void(lapack_int* n, lapack_int* ilo, lapack_int* ihi,
                      ValueType* a, lapack_int* lda, ValueType* tau,
                      ValueType* work, lapack_int* lwork, lapack_int* info);

  inline static FnType* fn = nullptr;

  static ::xla::ffi::Error Kernel(::xla::ffi::Buffer<dtype> x, lapack_int low,
                                  lapack_int high,
                                  ::xla::ffi::ResultBuffer<dtype> x_out,
                                  ::xla::ffi::ResultBuffer<dtype> tau,
                                  ::xla::ffi::ResultBuffer<LapackIntDtype> info,
                                  ::xla::ffi::ResultBuffer<dtype> work);

  static int64_t GetWorkspaceSize(lapack_int x_rows, lapack_int x_cols,
                                  lapack_int low, lapack_int high);
};

// lapack sytrd/hetrd

// Reduces a Symmetric (or Hermitian) square matrix to a tridiagonal form.
template <::xla::ffi::DataType dtype>
struct TridiagonalReduction {
  using ValueType = ::xla::ffi::NativeType<dtype>;
  using RealType = ::xla::ffi::NativeType<::xla::ffi::ToReal(dtype)>;
  using FnType = void(char* uplo, lapack_int* n, ValueType* a, lapack_int* lda,
                      RealType* d, RealType* e, ValueType* tau, ValueType* work,
                      lapack_int* lwork, lapack_int* info);

  inline static FnType* fn = nullptr;

  static ::xla::ffi::Error Kernel(
      ::xla::ffi::Buffer<dtype> x, MatrixParams::UpLo uplo,
      ::xla::ffi::ResultBuffer<dtype> x_out,
      ::xla::ffi::ResultBuffer<dtype> tau,
      ::xla::ffi::ResultBuffer<::xla::ffi::ToReal(dtype)> diagonal,
      ::xla::ffi::ResultBuffer<::xla::ffi::ToReal(dtype)> off_diagonal,
      ::xla::ffi::ResultBuffer<LapackIntDtype> info,
      ::xla::ffi::ResultBuffer<dtype> work);

  static int64_t GetWorkspaceSize(lapack_int x_rows, lapack_int x_cols);
};

}  // namespace jax

#endif  // JAXLIB_CPU_LAPACK_KERNELS_H_

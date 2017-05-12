// @HEADER
// ***********************************************************************
//
//          Tpetra: Templated Linear Algebra Services Package
//                 Copyright (2008) Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Michael A. Heroux (maherou@sandia.gov)
//
// ************************************************************************
// @HEADER

#ifndef TPETRA_DETAILS_PACKCRSMATRIX_HPP
#define TPETRA_DETAILS_PACKCRSMATRIX_HPP

#include "TpetraCore_config.h"
#include "Teuchos_Array.hpp"
#include "Teuchos_ArrayView.hpp"
#include "Tpetra_Details_OrdinalTraits.hpp"
#include "Tpetra_Details_computeOffsets.hpp"
#include "Kokkos_Core.hpp"
#include <memory>
#include <string>

/// \file Tpetra_Details_packCrsMatrix.hpp
/// \brief Functions for packing the entries of a Tpetra::CrsMatrix
///   for communication, in the case where it is valid to go to the
///   KokkosSparse::CrsMatrix (local sparse matrix data structure)
///   directly.
/// \warning This file, and its contents, are implementation details
///   of Tpetra.  The file itself or its contents may disappear or
///   change at any time.

namespace Tpetra {

#ifndef DOXYGEN_SHOULD_SKIP_THIS
// Forward declaration of Distributor
class Distributor;
#endif // DOXYGEN_SHOULD_SKIP_THIS

//
// Users must never rely on anything in the Details namespace.
//
namespace Details {


/// \brief Compute number of packets per row of the local matrix
///
/// \tparam GO Global ordinal type
/// \tparam ViewType Type of the Kokkos::View specialization
///   used to store the number of packets; the output array of this function.
/// \tparam ExportLIDsViewType Type of the Kokkos::View specialization
///   used to store the LIDs of local rows; the input array of this function.
/// \tparam LocalMatrixType Type of the KokkosSparse::CrsMatrix local matrix
///
template<class GO, class ViewType, class ExportLIDsViewType, class LocalMatrixType>
struct NumPacketsFunctor {

  typedef typename ViewType::value_type value_type;
  typedef typename LocalMatrixType::row_map_type row_map_type;
  typedef typename LocalMatrixType::value_type IST;
  typedef typename LocalMatrixType::ordinal_type LO;

  ViewType num_packets;
  ExportLIDsViewType export_lids;
  row_map_type row_map;

  NumPacketsFunctor(ViewType num_packets_, ExportLIDsViewType export_lids_,
      LocalMatrixType local_matrix):
    num_packets(num_packets_), export_lids(export_lids_),
    row_map(local_matrix.graph.row_map) {};

  KOKKOS_INLINE_FUNCTION
    void operator() (size_t i) const {
      LO export_lid = static_cast<LO>(export_lids[i]);
      const size_t count = static_cast<size_t>(row_map[export_lid+1]
                                             - row_map[export_lid]);
      const value_type num_bytes = sizeof(LO) + count * (sizeof(IST) + sizeof(GO));
      num_packets[i] = (count == 0) ? 0 : num_bytes;
    }
};

/// \brief Reduction result for PackCrsMatrixFunctor below.
///
/// The reduction result finds the offset and number of bytes associated with
/// the first out of bounds error or packing error occurs.
struct PackCrsMatrixError {

  size_t bad_index; // only valid if outOfBounds == true.
  size_t bad_offset;   // only valid if outOfBounds == true.
  size_t bad_num_bytes; // only valid if outOfBounds == true.
  bool out_of_bounds_error;
  bool packing_error;

  PackCrsMatrixError():
    bad_index(std::numeric_limits<size_t>::max()),
    bad_offset(0),
    bad_num_bytes(0),
    out_of_bounds_error(false),
    packing_error(false) {}

  KOKKOS_INLINE_FUNCTION bool success() const
  {
    return ((!out_of_bounds_error) && (!packing_error));
  }

  KOKKOS_INLINE_FUNCTION std::string summary() const
  {
    std::ostringstream os;
    os << "firstBadIndex: " << bad_index
       << ", firstBadOffset: " << bad_offset
       << ", firstBadNumBytes: " << bad_num_bytes
       << ", outOfBounds: " << (out_of_bounds_error ? "true" : "false");
    return os.str();
  }
};

template<class ViewType, class OffsetsViewType,
         class ExportsViewType, class ExportLIDsViewType,
         class LocalMatrixType, class LocalMapType>
struct PackCrsMatrixFunctor {

  typedef typename LocalMapType::local_ordinal_type LO;
  typedef typename LocalMapType::global_ordinal_type GO;
  typedef typename LocalMatrixType::value_type IST;
  typedef PackCrsMatrixError value_type;

  static_assert (std::is_same<LO, typename LocalMatrixType::ordinal_type>::value,
                 "LocalMapType::local_ordinal_type and "
                 "LocalMatrixType::ordinal_type must be the same.");

  ViewType num_packets_per_lid;
  OffsetsViewType offsets;
  ExportsViewType exports;
  ExportLIDsViewType export_lids;
  LocalMatrixType local_matrix;
  LocalMapType local_col_map;

  PackCrsMatrixFunctor(ViewType num_packets_per_lid_, OffsetsViewType offsets_,
      ExportsViewType exports_, ExportLIDsViewType export_lids_,
      LocalMatrixType local_matrix_, LocalMapType local_col_map_):
    num_packets_per_lid(num_packets_per_lid_), offsets(offsets_),
    exports(exports_), export_lids(export_lids_), local_matrix(local_matrix_),
    local_col_map(local_col_map_)
  {}

  KOKKOS_INLINE_FUNCTION void init(value_type& dst) const
  {
    dst.bad_index = std::numeric_limits<size_t>::max();
    dst.bad_offset = 0;
    dst.bad_num_bytes = 0;
    dst.out_of_bounds_error = false;
    dst.packing_error = false;
  }

  KOKKOS_INLINE_FUNCTION void
  join (volatile value_type& dst, const volatile value_type& src) const
  {
    if (src.bad_index < dst.bad_index) {
      dst.bad_index = src.bad_index;
      dst.bad_offset = src.bad_offset;
      dst.bad_num_bytes = src.bad_num_bytes;
      dst.out_of_bounds_error = src.out_of_bounds_error;
      dst.packing_error = src.packing_error;
    }
  }

  KOKKOS_INLINE_FUNCTION
  void operator()(LO i, value_type& dst) const
  {
    const LO export_lid = export_lids[i];
    const size_t buf_size = exports.size();

    // FIXME (mfh 24 Mar 2017) Everything here assumes UVM.  If
    // we want to fix that, we need to write a pack kernel for
    // the whole matrix, that runs on device.
    size_t num_ent = static_cast<size_t>(local_matrix.graph.row_map[export_lid+1]
                                       - local_matrix.graph.row_map[export_lid]);

    // Only pack this row's data if it has a nonzero number of
    // entries.  We can do this because receiving processes get the
    // number of packets, and will know that zero packets means zero
    // entries.

    if (num_ent != 0) {

      char* const num_ent_beg = exports.ptr_on_device() + offsets(i);
      char* const num_ent_end = num_ent_beg + sizeof (LO);
      char* const val_beg = num_ent_end;
      char* const val_end = val_beg + num_ent * sizeof (IST);
      char* const ind_beg = val_end;
      const size_t num_bytes = num_packets_per_lid(i);

      if ((offsets(i) > buf_size || offsets(i) + num_bytes > buf_size)) {
        dst.out_of_bounds_error = true;
      }
      else {
        dst.packing_error = ! packCrsMatrixRow(local_matrix, local_col_map,
            num_ent_beg, val_beg, ind_beg, num_ent, export_lid);
      }
      if (dst.out_of_bounds_error || dst.packing_error) {
        dst.bad_index = i;
        dst.bad_offset = offsets(i);
        dst.bad_num_bytes = num_bytes;
      }
    }
  }
};

template<class LocalMatrixType>
bool
crsMatrixAllocatePackSpace (const LocalMatrixType& lclMatrix,
                            std::unique_ptr<std::string>& errStr,
                            Teuchos::Array<char>& exports,
                            size_t& totalNumEntries,
                            const Teuchos::ArrayView<const typename LocalMatrixType::ordinal_type>& exportLIDs,
                            const size_t sizeOfGlobalOrdinal)
{
  typedef typename LocalMatrixType::ordinal_type LO;
  typedef typename LocalMatrixType::value_type IST;

  // The number of export LIDs must fit in LocalOrdinal, assuming
  // that the LIDs are distinct and valid on the calling process.
  const LO numExportLIDs = static_cast<LO> (exportLIDs.size ());

  bool ok = true; // true if and only if no errors occurred

  // Count the total number of matrix entries to send.
  totalNumEntries = 0;
  for (LO i = 0; i < numExportLIDs; ++i) {
    const LO lclRow = exportLIDs[i];
    size_t curNumEntries;
    curNumEntries = static_cast<size_t> (lclMatrix.graph.row_map[lclRow+1] -
                                         lclMatrix.graph.row_map[lclRow]);

    // FIXME (mfh 25 Jan 2015) We should actually report invalid row
    // indices as an error.  Just consider them nonowned for now.
    if (curNumEntries == Tpetra::Details::OrdinalTraits<size_t>::invalid ()) {
      curNumEntries = 0;
      ok = false;
    }
    totalNumEntries += curNumEntries;
  }

  if (! ok) {
    const std::string err ("Encountered invalid (flag) number of "
                           "row entries in one or more rows.");
    if (errStr.get () == NULL) {
      errStr = std::unique_ptr<std::string> (new std::string (err));
    }
    else {
      *errStr = err;
    }
    return false;
  }

  // FIXME (mfh 24 Feb 2013, 24 Mar 2017) This code is only correct
  // if sizeof(IST) is a meaningful representation of the amount of
  // data in a Scalar instance.  (LO and GO are always built-in
  // integer types.)
  //
  // Allocate the exports array.  It does NOT need padding for
  // alignment, since we use memcpy to write to / read from send /
  // receive buffers.
  const size_t allocSize =
    static_cast<size_t> (numExportLIDs) * sizeof (LO) +
    totalNumEntries * (sizeof (IST) + sizeOfGlobalOrdinal);
  if (static_cast<size_t> (exports.size ()) < allocSize) {
    exports.resize (allocSize);
  }
  return true;
}

/// \brief Packs a single row of the CrsMatrix.
///
/// Data (bytes) describing the row of the CRS matrix are "packed"
/// (concatenated) in to a single char* as
///
///   LO number of entries  \
///   GO column indices      > -- number of entries | column indices | values --
///   SC values             /
///
/// \tparam LocalMatrixType the specialization of the KokkosSparse::CrsMatrix
///   local matrix
/// \tparam LocalMapType the type of the local column map
template<class LocalMatrixType, class LocalMapType>
bool
packCrsMatrixRow (const LocalMatrixType& lclMatrix,
                  const LocalMapType& lclColMap,
                  char* const numEntOut,
                  char* const valOut,
                  char* const indOut,
                  const size_t numEnt, // number of entries in row
                  const typename LocalMatrixType::ordinal_type lclRow)
{
  using Kokkos::subview;
  typedef LocalMatrixType local_matrix_type;
  typedef LocalMapType local_map_type;
  typedef typename local_matrix_type::value_type IST;
  typedef typename local_matrix_type::ordinal_type LO;
  typedef typename local_matrix_type::size_type offset_type;
  typedef typename local_map_type::global_ordinal_type GO;
  typedef Kokkos::pair<offset_type, offset_type> pair_type;

  const LO numEntLO = static_cast<LO> (numEnt);
  memcpy (numEntOut, &numEntLO, sizeof (LO));

  if (numEnt == 0) {
    return true; // nothing more to pack
  }

#ifdef HAVE_TPETRA_DEBUG
  if (lclRow >= lclMatrix.numRows () ||
      (static_cast<size_t> (lclRow + 1) >=
       static_cast<size_t> (lclMatrix.graph.row_map.dimension_0 ()))) {
#else // NOT HAVE_TPETRA_DEBUG
  if (lclRow >= lclMatrix.numRows ()) {
#endif // HAVE_TPETRA_DEBUG
    // It's bad if this is not a valid local row index.  One thing
    // we can do is just pack the flag invalid value for the column
    // indices.  That makes sure that the receiving process knows
    // something went wrong.
    const GO flag = Tpetra::Details::OrdinalTraits<GO>::invalid ();
    for (size_t k = 0; k < numEnt; ++k) {
      memcpy (indOut + k * sizeof (GO), &flag, sizeof (GO));
    }
    // The values don't actually matter, but we might as well pack
    // something here.
    const IST zero = Kokkos::ArithTraits<IST>::zero ();
    for (size_t k = 0; k < numEnt; ++k) {
      memcpy (valOut + k * sizeof (GO), &zero, sizeof (GO));
    }
    return false;
  }

  // FIXME (mfh 24 Mar 2017) Everything here assumes UVM.  If we
  // want to fix that, we need to write a pack kernel for the whole
  // matrix, that runs on device.

  // Since the matrix is locally indexed on the calling process, we
  // have to use its column Map (which it _must_ have in this case)
  // to convert to global indices.
  const offset_type rowBeg = lclMatrix.graph.row_map[lclRow];
  const offset_type rowEnd = lclMatrix.graph.row_map[lclRow + 1];

  auto indIn = subview (lclMatrix.graph.entries, pair_type (rowBeg, rowEnd));
  auto valIn = subview (lclMatrix.values, pair_type (rowBeg, rowEnd));

  // Copy column indices one at a time, so that we don't need
  // temporary storage.
  for (size_t k = 0; k < numEnt; ++k) {
    const GO gblIndIn = lclColMap.getGlobalElement (indIn[k]);
    memcpy (indOut + k * sizeof (GO), &gblIndIn, sizeof (GO));
  }
  memcpy (valOut, valIn.ptr_on_device (), numEnt * sizeof (IST));
  return true;
}

/// \brief Pack specified entries of the given local sparse matrix for
///   communication.
///
/// \warning This is an implementation detail of Tpetra::CrsMatrix.
///
/// \param errStr [in/out] If an error occurs on any participating
///   process, allocate this if it is null, then fill the string with
///   local error reporting.  This is purely local to the process that
///   reports the error.  The caller is responsible for synchronizing
///   across processes.
///
/// \param exports [in/out] Output pack buffer; resized if needed.
///
/// \param numPacketsPerLID [out] Entry k gives the number of bytes
///   packed for row exportLIDs[k] of the local matrix.
///
/// \param exportLIDs [in] Local indices of the rows to pack.
///
/// \param lclMatrix [in] The local sparse matrix to pack.
///
/// \return true if no errors occurred on the calling process, else
///   false.  This is purely local to the process that discovered the
///   error.  The caller is responsible for synchronizing across
///   processes.
template<class LocalMatrixType, class LocalMapType>
bool
packCrsMatrix (const LocalMatrixType& lclMatrix,
               const LocalMapType& lclColMap,
               std::unique_ptr<std::string>& errStr,
               Teuchos::Array<char>& exports,
               const Teuchos::ArrayView<size_t>& numPacketsPerLID,
               size_t& constantNumPackets,
               const Teuchos::ArrayView<const typename LocalMatrixType::ordinal_type>& exportLIDs,
               const int myRank,
               Distributor& /* dist */)
{
  using Kokkos::View;
  using Kokkos::HostSpace;
  using Kokkos::MemoryUnmanaged;
  using ::Tpetra::Details::computeOffsetsFromCounts;
  typedef LocalMatrixType local_matrix_type;
  typedef LocalMapType local_map_type;
  typedef typename LocalMapType::local_ordinal_type LO;
  typedef typename LocalMapType::global_ordinal_type GO;

  static_assert (std::is_same<LO, typename LocalMatrixType::ordinal_type>::value,
                 "LocalMapType::local_ordinal_type and "
                 "LocalMatrixType::ordinal_type must be the same.");

  const size_t numExportLIDs = static_cast<size_t> (exportLIDs.size ());
  if (numExportLIDs != static_cast<size_t> (numPacketsPerLID.size ())) {
    std::ostringstream os;
    os << "exportLIDs.size() = " << numExportLIDs
       << " != numPacketsPerLID.size() = " << numPacketsPerLID.size () << "."
       << std::endl;
    if (errStr.get () == NULL) {
      errStr = std::unique_ptr<std::string> (new std::string ());
    }
    *errStr = os.str ();
    return false;
  }

  // Setting this to zero tells the caller to expect a possibly
  // different ("nonconstant") number of packets per local index
  // (i.e., a possibly different number of entries per row).
  constantNumPackets = 0;

  // The pack buffer 'exports' enters this method possibly
  // unallocated.  Do the first two parts of "Count, allocate, fill,
  // compute."
  size_t totalNumEntries = 0;
  const bool allocOK =
    crsMatrixAllocatePackSpace (lclMatrix, errStr, exports, totalNumEntries,
                                exportLIDs, sizeof (GO));
  if (! allocOK) {
    const std::string err =
      std::string ("packCrsMatrix: Allocating pack space failed: ") + *errStr;
    if (errStr.get () == NULL) {
      errStr = std::unique_ptr<std::string> (new std::string (err));
    }
    else {
      *errStr = err;
    }
    return false;
  }

  // Get the number of packets on each row.
  typedef size_t count_type;
  typedef View<const LO*, HostSpace, MemoryUnmanaged> LIVT;  // Local indices view type
  typedef View<count_type*, HostSpace, MemoryUnmanaged> CVT; // Counts view type
  typedef View<char*, HostSpace, MemoryUnmanaged> EXVT; // Exports view type
  // @mfh: Is this the right device type?
  typedef typename local_matrix_type::device_type device_type;
  typedef typename device_type::execution_space execution_space;
  typedef Kokkos::RangePolicy<execution_space, LO> range_type;

  CVT num_packets_per_lid(numPacketsPerLID.getRawPtr(), numPacketsPerLID.size());
  LIVT export_lids(exportLIDs.getRawPtr(), exportLIDs.size());
  EXVT exports_v(exports.getRawPtr(), exports.size());

  typedef NumPacketsFunctor<GO, CVT, LIVT, local_matrix_type> np_functor_type;
  np_functor_type np_functor(num_packets_per_lid, export_lids, lclMatrix);
  Kokkos::parallel_for(range_type(0, numExportLIDs), np_functor);

  // Now get the offsets
  typedef size_t offset_type;
  typedef View<offset_type*, HostSpace> OVT; // Offsets view type
  OVT offsets("packCrsMatrix: offsets", numExportLIDs+1);
  computeOffsetsFromCounts(offsets, num_packets_per_lid);

  // Now do the actual pack!
  typedef PackCrsMatrixFunctor<CVT, OVT, EXVT, LIVT,
          local_matrix_type, local_map_type> pack_functor_type;
  typename pack_functor_type::value_type result;
  pack_functor_type pack_functor(num_packets_per_lid, offsets, exports_v,
      export_lids, lclMatrix, lclColMap);
  Kokkos::parallel_reduce(range_type(0, numExportLIDs), pack_functor, result);

  if (!result.success()) {
    std::ostringstream os;
    os << "Proc " << myRank << ": packCrsMatrix failed.  "
       << result.summary()
       << std::endl;
    if (errStr.get () != NULL) {
      errStr = std::unique_ptr<std::string> (new std::string ());
      *errStr = os.str ();
    }
  }

  return result.success();
}

} // namespace Details
} // namespace Tpetra

#endif // TPETRA_DETAILS_PACKCRSMATRIX_HPP

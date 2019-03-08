/*
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
*/

#include "Tpetra_TestingUtilities.hpp"
#include "Tpetra_Core.hpp"
#include "Tpetra_Map.hpp"
#include "Tpetra_ConfigDefs.hpp"
#include "Tpetra_Details_crsUtils.hpp"

#include "Kokkos_Core.hpp"
#include "Teuchos_UnitTestHarness.hpp"
#include "Teuchos_Array.hpp"
#include "Teuchos_as.hpp"

#include <algorithm>
#include <iterator>

using Teuchos::CommandLineProcessor;
using Tpetra::Details::padCrsArrays;
using Tpetra::Details::insertCrsIndices;
using Tpetra::Details::impl::ind_difference;
using Tpetra::Details::impl::uninitialized_view;
using std::vector;

namespace {

  TEUCHOS_STATIC_SETUP()
  {
    Teuchos::CommandLineProcessor &clp = Teuchos::UnitTestRepository::getCLP();
    clp.addOutputSetupOptions(true);
  }

  //
  // UNIT TESTS
  //
TEUCHOS_UNIT_TEST(CrsGraph, ResizeRowPointersAndIndices_1)
{
  using device_type = typename Tpetra::Map<>::device_type;
  using execution_space = typename device_type::execution_space;
  using ordinal_type = size_t;
  using view_type = Kokkos::View<ordinal_type*, device_type>;
  using size_type = typename view_type::size_type;

  ordinal_type num_row = 4;
  ordinal_type num_indices_per_row = 5;
  ordinal_type num_indices = num_indices_per_row * num_row;
  auto row_ptrs_beg = uninitialized_view<view_type>("beg", num_row+1);
  // this assumes UVM
  for (ordinal_type i=0; i<num_row+1; i++) row_ptrs_beg(i) = num_indices_per_row*i;

  auto row_ptrs_end = uninitialized_view<view_type>("end", num_row);
  for (ordinal_type i=0; i<num_row; i++) row_ptrs_end(i) = row_ptrs_beg(i+1);

  auto indices = uninitialized_view<view_type>("indices", num_indices);
  for (ordinal_type i=0; i<num_row; i++) {
    auto start = row_ptrs_beg(i);
    auto end = row_ptrs_beg(i+1);
    for (ordinal_type j=start, k=0; j<end; j++, k++) {
      indices(j) = (i + 1) * num_indices_per_row + k;
    }
  }

  auto import_lids = uninitialized_view<view_type>("import lids", num_row);
  auto num_packets_per_lid = uninitialized_view<view_type>("num packets", num_row);
  for (ordinal_type i=0; i<num_row; i++) {
   import_lids(i) = i;
   num_packets_per_lid(i) = i;
  }
  ordinal_type num_extra =
    num_row*(num_packets_per_lid(0) + num_packets_per_lid(num_row-1))/2;

  Kokkos::UnorderedMap<ordinal_type,ordinal_type,device_type> padding(import_lids.size());
  execution_space::fence();
  for (size_type i=0; i<import_lids.size(); i++){
    padding.insert(import_lids(i), num_packets_per_lid(i));
  }
  execution_space::fence();
  TEST_ASSERT(!padding.failed_insert());

  padCrsArrays(row_ptrs_beg, row_ptrs_end, indices, padding);
  TEST_ASSERT(indices.size() == static_cast<size_type>(num_indices + num_extra));

  {
    // make sure row_ptrs_beg is where it should be
    bool offsets_ok = row_ptrs_beg(0) == 0;
    for (ordinal_type i=1; i<num_row; i++) {
      auto expected = row_ptrs_beg(i-1) + num_indices_per_row + num_packets_per_lid(i-1);
      if (row_ptrs_beg(i) != expected) offsets_ok = false;
    }
    if (offsets_ok) offsets_ok = row_ptrs_beg(num_row) == indices.size();
    TEST_ASSERT(offsets_ok);
  }

  {
    // make sure indices were shifted correctly
    bool indices_ok = true;
    for (ordinal_type i=0; i<num_row; i++) {
      auto start = row_ptrs_beg(i);
      auto end = row_ptrs_end(i);
      for (ordinal_type j=start, k=0; j<end; j++, k++) {
        auto expected = (i + 1) * num_indices_per_row + k;
        if (expected != indices(j)) indices_ok = false;
      }
    }
    TEST_ASSERT(indices_ok);
  }
}

TEUCHOS_UNIT_TEST(CrsGraph, ResizeRowPointersAndIndices_2)
{
  typedef typename Tpetra::Map<>::device_type device_type;
  using execution_space = typename device_type::execution_space;
  using ordinal_type = size_t;
  using view_type = Kokkos::View<ordinal_type*, device_type>;
  using size_type = typename view_type::size_type;

  auto row_ptrs_beg = view_type("beg", 4);
  auto row_ptrs_end = view_type("end", 3);
  auto indices = view_type("indices", 9);

  // Row 1, 3 allocations, 3 used
  row_ptrs_beg(0) = 0; row_ptrs_end(0) = 3;
  indices(0) = 1; indices(1) = 2; indices(2) = 3;  // Row 1

  // Row 2, 3 allocations only 1 used
  row_ptrs_beg(1) = 3; row_ptrs_end(1) = 4;
  indices(3) = 4;

  // Row 3, 3 allocations, only 2 used
  row_ptrs_beg(2) = 6; row_ptrs_end(2) = 8;
  indices(6) = 7; indices(7) = 8;

  row_ptrs_beg(3) = 9;

  // Import 5 extra values in to Row 1 and 3 extra in to Row 3
  auto import_lids = view_type("import lids", 2);
  auto num_packets_per_lid = view_type("num packets", 2);

  // Import LIDs not ordered
  import_lids(0) = 2;
  num_packets_per_lid(0) = 3;

  import_lids(1) = 0;
  num_packets_per_lid(1) = 5;

  Kokkos::UnorderedMap<ordinal_type,ordinal_type,device_type> padding(import_lids.size());
  execution_space::fence();
  for (size_type i=0; i<import_lids.size(); i++){
    padding.insert(import_lids(i), num_packets_per_lid(i));
  }
  execution_space::fence();
  TEST_ASSERT(!padding.failed_insert());
  padCrsArrays(row_ptrs_beg, row_ptrs_end, indices, padding);

  // Check row offsets
  TEST_ASSERT(row_ptrs_beg(0) == 0);
  TEST_ASSERT(row_ptrs_beg(1) == 8);
  TEST_ASSERT(row_ptrs_beg(2) == 11);
  TEST_ASSERT(row_ptrs_beg(3) == 16);
  TEST_ASSERT(indices.size() == 16);

  TEST_ASSERT(row_ptrs_end(0) == 3);
  TEST_ASSERT(row_ptrs_end(1) == 9);
  TEST_ASSERT(row_ptrs_end(2) == 13);

  // Row 1
  TEST_ASSERT(indices(0) == 1);
  TEST_ASSERT(indices(1) == 2);
  TEST_ASSERT(indices(2) == 3);
  // 5 extra entries in Row 1

  // Row 2
  TEST_ASSERT(indices(8) == 4);
  // 2 unused indices in Row 2

  // Row 2
  TEST_ASSERT(indices(11) == 7);
  TEST_ASSERT(indices(12) == 8);

}

template <class V1, class V2>
bool
compare_array_values(V1 const& arr1, V2 const& arr2, size_t const n)
{
  bool l_success = true;
  for (size_t i = 0; i < n; i++)
  {
    if (arr1[i] != arr2[i])
    {
      l_success = false;
      //std::cout << "ARR1[" << i << "] = " << arr1[i] << ", expected " << arr2[i] << "\n";
    }
  }
  return l_success;
}

TEUCHOS_UNIT_TEST( TpetraUtils, insertIndices )
{
  {
    vector<int> indices{1, 3, 5, 7, -1, -1, -1, -1, -1, -1};
    size_t num_assigned = 4;
    vector<int> in_indices{0, 2, 4, 6, 8, 7, 5, 3, 1, 2, 6, 8, 4, 0};
    auto num_inserted = insertCrsIndices(indices, num_assigned, in_indices);
    TEST_EQUALITY(num_inserted, 5);
    vector<int> expected{1, 3, 5, 7, 0, 2, 4, 6, 8};
    TEST_ASSERT(compare_array_values(indices, expected, expected.size()));
  }

  {
    vector<int> indices{3, 6, 9, 12, -1, -1, -1, -1};
    size_t num_assigned = 4;
    vector<int> in_indices{1, 0, 2};
    vector<int> expected{3, 6, 9, 12, 1, 0, 2, -1};
    auto num_inserted = insertCrsIndices(indices, num_assigned, in_indices);
    TEST_EQUALITY(num_inserted, in_indices.size());
    TEST_ASSERT(compare_array_values(indices, expected, expected.size()));
  }

  {
    vector<int> indices{3, 6, 9, 12, -1, -1, -1};
    size_t num_assigned = 4;
    vector<int> in_indices{1, 0, 2, 5};
    auto num_inserted = insertCrsIndices(indices, num_assigned, in_indices);
    TEST_EQUALITY(num_inserted, -1);
  }

  {
    vector<int> indices{3, 6, 9, 12, -1, -1, -1, -1, -1};
    size_t num_assigned = 4;
    vector<int> in_indices{0, 4, 7, 10, 13};
    vector<int> expected{3, 6, 9, 12, 0, 4, 7, 10, 13};
    auto num_inserted = insertCrsIndices(indices, num_assigned, in_indices);
    TEST_EQUALITY(num_inserted, in_indices.size());
    TEST_ASSERT(compare_array_values(indices, expected, expected.size()));
  }
}

TEUCHOS_UNIT_TEST( TpetraUtils, insertIndicesWithCallback )
{
  {
    vector<int> indices{3, 6, 9, 12, -1, -1, -1};
    size_t num_assigned = 4;
    vector<int> in_indices{1, 0, 2, 2, 1, 0, 1, 2, 1};
    vector<int> expected{3, 6, 9, 12, 1, 0, 2};
    vector<int> values(indices.size(), 0);
    vector<int> expected_values{0, 0, 0, 0, 4, 2, 3};
    auto num_inserted = insertCrsIndices(indices, num_assigned, in_indices,
                                         [&](const int offset){values[offset] += 1; });
    TEST_EQUALITY(num_inserted, 3);
    TEST_ASSERT(compare_array_values(indices, expected, expected.size()));
    TEST_ASSERT(compare_array_values(values, expected_values, expected_values.size()));
  }
}

TEUCHOS_UNIT_TEST( TpetraCrsUtils, ind_difference )
{
  vector<int> indices{0, 3, 6, 9, 12, -1, -1, -1};
  size_t num_assigned = 5;
  vector<int> in_indices{1, 2, 5, 7, 8, 9, 12, 13, 14};
  vector<int> expected{1, 2, 5, 7, 8, 13, 14};
  auto diff =
    ind_difference(in_indices.data(), in_indices.size(), indices.data(), num_assigned);
  TEST_COMPARE_ARRAYS(diff, expected);
}

} // namespace (anonymous)

int
main (int argc, char* argv[])
{
  // Initialize MPI (if enabled) before initializing Kokkos.  This
  // lets MPI control things like pinning processes to sockets.
  Tpetra::ScopeGuard tpetraScope (&argc, &argv);
  const int errCode =
    Teuchos::UnitTestRepository::runUnitTestsFromMain (argc, argv);
  return errCode;
}


TRIBITS_ADD_EXECUTABLE_AND_TEST(
  Directory_UnitTests
  SOURCES
    Directory_UnitTests
    ${TEUCHOS_STD_UNIT_TEST_MAIN}
  COMM serial mpi
  STANDARD_PASS_OUTPUT
  )

TRIBITS_ADD_EXECUTABLE_AND_TEST(
  Directory_Issue6987
  SOURCES
    Directory_Issue6987
  COMM mpi
  NUM_MPI_PROCS 4
  STANDARD_PASS_OUTPUT
  )

TRIBITS_ADD_EXECUTABLE_AND_TEST(
  Directory_Issue7223
  SOURCES
    Directory_Issue7223
    ${TEUCHOS_STD_UNIT_TEST_MAIN}
  COMM mpi
  STANDARD_PASS_OUTPUT
  )

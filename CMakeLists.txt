cmake_minimum_required( VERSION 2.8 )
project( StickySat )

#optional add c++11 support for eclipse
if(ECLIPSE)
	add_definitions(-D__cplusplus=201103L)
endif(ECLIPSE)

set(CC_TARGETS 
"${CMAKE_CURRENT_SOURCE_DIR}/glucose/core/Solver.cc"
"${CMAKE_CURRENT_SOURCE_DIR}/glucose/simp/SimpSolver.cc"
"${CMAKE_CURRENT_SOURCE_DIR}/glucose/utils/Options.cc"
"${CMAKE_CURRENT_SOURCE_DIR}/glucose/utils/System.cc"
"${CMAKE_CURRENT_SOURCE_DIR}/glucose/mtl/XAlloc.cc"

"${CMAKE_CURRENT_SOURCE_DIR}/parallel_utils/CPUBind.cc"

"${CMAKE_CURRENT_SOURCE_DIR}/shared/ClauseBucketArray.cc"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/ClauseDatabase.cc"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/ClauseWatcher.cc"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/ClauseReducer.cc"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/ClauseTypes.cc"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/CoreSolver.cc"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/DatabaseThreadState.cc"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/Heuristic.cc"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/LiteralSetting.cc"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/Main.cc"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/PropagateResult.cc"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/ReferenceSharer.cc"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/SolverConfiguration.cc"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/SharedSolver.cc"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/Statistic.cc"
)

set(H_TARGETS 
"${CMAKE_CURRENT_SOURCE_DIR}/glucose/core/BoundedQueue.h"
"${CMAKE_CURRENT_SOURCE_DIR}/glucose/core/Constants.h"
"${CMAKE_CURRENT_SOURCE_DIR}/glucose/core/Dimacs.h"
"${CMAKE_CURRENT_SOURCE_DIR}/glucose/core/Solver.h"
"${CMAKE_CURRENT_SOURCE_DIR}/glucose/core/SolverTypes.h"
"${CMAKE_CURRENT_SOURCE_DIR}/glucose/mtl/Alg.h"
"${CMAKE_CURRENT_SOURCE_DIR}/glucose/mtl/Alloc.h"
"${CMAKE_CURRENT_SOURCE_DIR}/glucose/mtl/Clone.h"
"${CMAKE_CURRENT_SOURCE_DIR}/glucose/mtl/Heap.h"
"${CMAKE_CURRENT_SOURCE_DIR}/glucose/mtl/IntTypes.h"
"${CMAKE_CURRENT_SOURCE_DIR}/glucose/mtl/Map.h"
"${CMAKE_CURRENT_SOURCE_DIR}/glucose/mtl/Queue.h"
"${CMAKE_CURRENT_SOURCE_DIR}/glucose/mtl/Sort.h"
"${CMAKE_CURRENT_SOURCE_DIR}/glucose/mtl/Vec.h"
"${CMAKE_CURRENT_SOURCE_DIR}/glucose/mtl/XAlloc.h"
"${CMAKE_CURRENT_SOURCE_DIR}/glucose/simp/SimpSolver.h"
"${CMAKE_CURRENT_SOURCE_DIR}/glucose/utils/Options.h"
"${CMAKE_CURRENT_SOURCE_DIR}/glucose/utils/ParseUtils.h"
"${CMAKE_CURRENT_SOURCE_DIR}/glucose/utils/System.h"

"${CMAKE_CURRENT_SOURCE_DIR}/parallel_utils/CPUBind.h"
"${CMAKE_CURRENT_SOURCE_DIR}/parallel_utils/CppOptional.h"
"${CMAKE_CURRENT_SOURCE_DIR}/parallel_utils/LockArray.h"
"${CMAKE_CURRENT_SOURCE_DIR}/parallel_utils/LockRingBuffer.h"
"${CMAKE_CURRENT_SOURCE_DIR}/parallel_utils/LockStack.h"
"${CMAKE_CURRENT_SOURCE_DIR}/parallel_utils/Timer.h"

"${CMAKE_CURRENT_SOURCE_DIR}/shared/ClauseBucketArray.h"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/ClauseDatabase.h"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/ClauseWatcher.h"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/ClauseReducer.h"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/ClauseTypes.h"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/CoreSolver.h"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/DatabaseThreadState.h"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/Heuristic.h"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/LiteralSetting.h"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/PropagateResult.h"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/SolverConfiguration.h"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/SharedSolver.h"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/SharedTypes.h"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/Statistic.h"
"${CMAKE_CURRENT_SOURCE_DIR}/shared/Statistic.h"
)

if( NOT CMAKE_BUILD_TYPE )
	set(CMAKE_BUILD_TYPE "Release")
endif( NOT CMAKE_BUILD_TYPE )

if( STICKY_BUCKET_SIZE )
	add_definitions(-DBUCKET_SIZE=${STICKY_BUCKET_SIZE})
	message(STATUS "Using bucket size of ${STICKY_BUCKET_SIZE} bytes (determines the maxmimal length of a clause" )
endif( STICKY_BUCKET_SIZE )


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
if(${CMAKE_BUILD_TYPE} STREQUAL "Release")
	message(STATUS "Excluding assertions")
  add_definitions(-DNDEBUG)
  if( INTEL_COMPILER )
    message(STATUS "Using intel compiler")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ipo -std=c++11")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -ipo")
  else( INTEL_COMPILER )
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto -fwhole-program -std=c++11")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto -fwhole-program")
  endif( INTEL_COMPILER)
else(${CMAKE_BUILD_TYPE} STREQUAL "Release")
	message(STATUS "Including assertions")
endif(${CMAKE_BUILD_TYPE} STREQUAL "Release")

set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/glucose")
set(LIBRARIES pthread z)

add_executable(sticky ${CC_TARGETS} ${H_TARGETS})
target_include_directories(sticky PUBLIC ${INCLUDE_DIR})
target_link_libraries(sticky ${LIBRARIES})


install(TARGETS sticky DESTINATION bin)




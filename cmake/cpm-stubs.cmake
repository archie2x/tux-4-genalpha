# Empty stubs so gersemi recognizes CPM's commands and stops lower-casing
# them as if they were unknown user calls. Bodies are intentionally empty;
# the real implementations live in CPM.cmake, downloaded by the bootstrap
# in cmake/modules/CPM.cmake.
#
# Referenced via super-project .gersemirc's `definitions:` list.

function(CPMAddPackage)
endfunction()

function(CPMFindPackage)
endfunction()

macro(CPMGetPackage Name)
endmacro()

macro(CPMDeclarePackage Name)
endmacro()

macro(CPMUsePackageLock file)
endmacro()

function(CPMRegisterPackage PACKAGE VERSION)
endfunction()

function(CPMGetPackageVersion PACKAGE OUTPUT)
endfunction()

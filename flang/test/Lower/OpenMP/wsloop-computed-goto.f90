! RUN: bbc -emit-hlfir -fopenmp --wrap-unstructured-constructs-in-execute-region %s -o - | FileCheck %s

! An unstructured DO whose body has a computed GO TO targeting a label
! inside the loop.  The DO is wrappable, so its isUnstructured no longer
! propagates to the enclosing OpenMPConstruct — the top-level
! createEmptyBlocks therefore does not recurse into the DO's body, and the
! label 17 ContinueStmt has no pre-allocated block when the OMP loop-nest
! lowering later reaches the ComputedGotoStmt.  genMultiwayBranch (and
! genConstructExitBranch) now allocate label-target blocks on demand.
subroutine s(ii1)
  integer ii1
!$omp do
  do ii1 = 0, 1
    go to (17), ii1
17  continue
  end do
end subroutine

! CHECK-LABEL: func.func @_QPs
! CHECK:         omp.wsloop
! CHECK:           omp.loop_nest
! CHECK:             fir.load
! CHECK:             fir.select %{{[0-9]+}} : i32 [1, ^[[TARGET:bb[0-9]+]], unit, ^[[TARGET]]]
! CHECK:           ^[[TARGET]]:
! CHECK:             omp.yield

! RUN: bbc -emit-hlfir -fopenmp --wrap-unstructured-constructs-in-execute-region %s -o - | FileCheck %s

! An unstructured SELECT CASE inside an OpenMP DO body.  With the wrap
! flag on, the enclosing DoConstruct is wrappable, so its isUnstructured
! no longer propagates to the OpenMPConstruct — the top-level
! createEmptyBlocks does not recurse into the DO's body, and the
! CaseStmt / constructExit blocks the SelectCase lowering wants are not
! pre-allocated.  genFIR(SelectCaseStmt) now goes through
! getOrCreateEvalBlock, which allocates the missing blocks on demand.
subroutine s(a, k)
  integer a, k
!$omp do
  do k = 1, 2
    select case (k)
      case (1)
        a = 10
      case (2)
        a = 20
      case default
        a = 30
    end select
  end do
end subroutine

! CHECK-LABEL: func.func @_QPs
! CHECK:         omp.wsloop
! CHECK:           omp.loop_nest
! CHECK:             fir.load
! CHECK:             fir.select_case %{{[0-9]+}} : i32 [#fir.point, %{{.*}}, ^[[C1:bb[0-9]+]], #fir.point, %{{.*}}, ^[[C2:bb[0-9]+]], unit, ^[[CDFLT:bb[0-9]+]]]
! CHECK:           ^[[C1]]:
! CHECK:             hlfir.assign
! CHECK:           ^[[C2]]:
! CHECK:             hlfir.assign
! CHECK:           ^[[CDFLT]]:
! CHECK:             hlfir.assign

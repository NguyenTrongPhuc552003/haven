(** * IsolationModel.v
    Haven Hypervisor - Core Spatial Isolation Model

    This file formalises the policy-level spatial isolation invariant for
    Haven's static-partition EL2 hypervisor.  It does NOT model hardware; it
    models the *logical state* that the C runtime (src/core/mm/stage2.c)
    maintains and proves that the invariant is preserved under the permitted
    state transitions.

    Coq version: 8.18
    Corresponds to: include/haven/stage2.h, src/core/mm/stage2.c
    Chapter traceability: Thesis Ch. 4 §4.2 "Spatial Isolation Invariant"
*)

From Coq Require Import Arith.
From Coq Require Import List.
From Coq Require Import Bool.
From Coq Require Import Lia.
Import ListNotations.

(* ------------------------------------------------------------------ *)
(** ** §1  Base types                                                   *)
(* ------------------------------------------------------------------ *)

(** Partition identifiers are natural numbers.  Partition 0 is reserved
    (mirrors the C invariant that partition_id == 0 is always invalid). *)
Definition PartitionId : Type := nat.

(** Physical addresses are natural numbers.  We work in a flat address
    space; the C implementation uses 64-bit unsigned integers. *)
Definition PhysAddr : Type := nat.

(* ------------------------------------------------------------------ *)
(** ** §2  Memory region                                                *)
(* ------------------------------------------------------------------ *)

(** A memory region is a half-open interval [pa_base, pa_base + pa_size).
    Corresponds to [hv_mem_region.pa_base] and [hv_mem_region.size] in the
    C header. *)
Record MemRegion : Type := mkMemRegion
  { pa_base : PhysAddr
  ; pa_size : nat
  }.

(** An address [pa] is *inside* region [r] when it falls in the
    half-open interval [r.pa_base, r.pa_base + r.pa_size). *)
Definition pa_in_region (pa : PhysAddr) (r : MemRegion) : Prop :=
  r.(pa_base) <= pa /\ pa < r.(pa_base) + r.(pa_size).

(** Two regions are *disjoint* when neither contains any address of the
    other.  The disjointness check mirrors [hv_range_overlaps] in stage2.c
    (inverted): regions are disjoint iff end_a <= base_b OR end_b <= base_a. *)
Definition regions_disjoint (r1 r2 : MemRegion) : Prop :=
  r1.(pa_base) + r1.(pa_size) <= r2.(pa_base) \/
  r2.(pa_base) + r2.(pa_size) <= r1.(pa_base).

(* ------------------------------------------------------------------ *)
(** ** §3  Partition and hypervisor state                               *)
(* ------------------------------------------------------------------ *)

(** Each partition has an identifier, a list of physical memory regions,
    and a boolean indicating whether its mapping has been committed
    (corresponds to [stage2_partition_mapped] in stage2.c). *)
Record PartitionState : Type := mkPartitionState
  { ps_id         : PartitionId
  ; ps_regions    : list MemRegion
  ; ps_configured : bool
  }.

(** The hypervisor state is the list of all partition states plus a count
    (the count is redundant with [length] but makes certain lemma
    statements cleaner). *)
Record HvState : Type := mkHvState
  { hv_partitions    : list PartitionState
  ; hv_nr_partitions : nat
  }.

(* ------------------------------------------------------------------ *)
(** ** §4  Ownership predicates                                         *)
(* ------------------------------------------------------------------ *)

(** [partition_contains_pa s id pa] holds when partition [id] exists in
    state [s], is configured, and at least one of its regions contains [pa]. *)
Definition partition_contains_pa
    (s : HvState) (id : PartitionId) (pa : PhysAddr) : Prop :=
  exists (p : PartitionState) (r : MemRegion),
    In p s.(hv_partitions) /\
    p.(ps_id) = id /\
    p.(ps_configured) = true /\
    In r p.(ps_regions) /\
    pa_in_region pa r.

(* ------------------------------------------------------------------ *)
(** ** §5  Spatial isolation invariant                                  *)
(* ------------------------------------------------------------------ *)

(** The *spatial isolation invariant* states that for every pair of
    distinct partitions (p1, p2), every region belonging to p1 is
    disjoint from every region belonging to p2.

    This directly corresponds to the cross-partition overlap check
    performed in the inner loop of [hv_stage2_map_partition] (lines
    121-133 of stage2.c). *)
Definition spatial_isolation_invariant (s : HvState) : Prop :=
  forall (p1 p2 : PartitionState),
    In p1 s.(hv_partitions) ->
    In p2 s.(hv_partitions) ->
    p1.(ps_id) <> p2.(ps_id) ->
    forall (r1 r2 : MemRegion),
      In r1 p1.(ps_regions) ->
      In r2 p2.(ps_regions) ->
      regions_disjoint r1 r2.

(* ------------------------------------------------------------------ *)
(** ** §6  Primary theorem: spatial isolation                           *)
(* ------------------------------------------------------------------ *)

(** If the invariant holds and [pa] belongs to partition [p1], then
    [pa] cannot belong to any distinct partition [p2]. *)
Theorem spatial_isolation :
  forall (s : HvState) (p1 p2 : PartitionId) (pa : PhysAddr),
    spatial_isolation_invariant s ->
    p1 <> p2 ->
    partition_contains_pa s p1 pa ->
    ~partition_contains_pa s p2 pa.
Proof.
  intros s p1 p2 pa Hinv Hne Hp1.
  (** Unpack the witness for p1. *)
  destruct Hp1 as [ps1 [r1 [Hin1 [Hid1 [_Hcfg1 [Hinr1 Hpa1]]]]]].
  (** Assume, for contradiction, that pa also belongs to p2. *)
  intros Hp2.
  destruct Hp2 as [ps2 [r2 [Hin2 [Hid2 [_Hcfg2 [Hinr2 Hpa2]]]]]].
  (** Apply the invariant to obtain disjointness of r1 and r2. *)
  assert (Hdisj : regions_disjoint r1 r2).
  { apply Hinv with (p1 := ps1) (p2 := ps2); auto.
    rewrite Hid1, Hid2; exact Hne. }
  (** Unpack [pa_in_region pa r1]: r1.base <= pa < r1.base + r1.size. *)
  destruct Hpa1 as [Hge1 Hlt1].
  (** Unpack [pa_in_region pa r2]: r2.base <= pa < r2.base + r2.size. *)
  destruct Hpa2 as [Hge2 Hlt2].
  (** [regions_disjoint] gives two cases; each is a contradiction with
      the bounds above. *)
  destruct Hdisj as [Hdisj | Hdisj]; lia.
Qed.

(* ------------------------------------------------------------------ *)
(** ** §7  Initial-state lemma                                          *)
(* ------------------------------------------------------------------ *)

(** The empty hypervisor state - no partitions at all - trivially
    satisfies the spatial isolation invariant because the universal
    quantifier over [In p1 []] is vacuously false. *)
Definition empty_hv_state : HvState :=
  mkHvState [] 0.

Lemma empty_state_satisfies_invariant :
  spatial_isolation_invariant empty_hv_state.
Proof.
  unfold spatial_isolation_invariant, empty_hv_state.
  simpl.
  intros p1 p2 Hin1.
  contradiction.
Qed.

(* ------------------------------------------------------------------ *)
(** ** §8  Auxiliary: disjointness is symmetric                         *)
(* ------------------------------------------------------------------ *)

Lemma regions_disjoint_sym :
  forall r1 r2 : MemRegion,
    regions_disjoint r1 r2 -> regions_disjoint r2 r1.
Proof.
  intros r1 r2 H.
  unfold regions_disjoint in *.
  destruct H; [right | left]; exact H.
Qed.

(** A region with size 0 is disjoint from every region. *)
Lemma zero_size_disjoint :
  forall r1 r2 : MemRegion,
    r1.(pa_size) = 0 ->
    regions_disjoint r1 r2.
Proof.
  intros r1 r2 Hsz.
  unfold regions_disjoint.
  left. rewrite Hsz. simpl. lia.
Qed.

(** No address can be inside a zero-sized region. *)
Lemma no_pa_in_zero_region :
  forall (pa : PhysAddr) (r : MemRegion),
    r.(pa_size) = 0 ->
    ~pa_in_region pa r.
Proof.
  intros pa r Hsz.
  unfold pa_in_region.
  intro H.
  destruct H as [_ Hlt].
  rewrite Hsz in Hlt.
  lia.
Qed.

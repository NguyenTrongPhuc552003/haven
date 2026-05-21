(** * Stage2Policy.v
    Haven Hypervisor - Stage-2 Mapping Policy Preservation

    This file formalises the [hv_stage2_map_partition] operation and
    proves that a well-checked mapping addition preserves the spatial
    isolation invariant defined in IsolationModel.v.

    Coq version: 8.18
    Corresponds to: src/core/mm/stage2.c (hv_stage2_map_partition)
    Chapter traceability: Thesis Ch. 4 §4.3 "Mapping Policy Correctness"
*)

From Coq Require Import Arith.
From Coq Require Import List.
From Coq Require Import Bool.
From Coq Require Import Lia.
Import ListNotations.

(** Re-export the base model.  In a real build this would be [Require
    Import IsolationModel] but since we keep the files independent we
    inline the definitions that are needed here, keeping them
    syntactically identical to IsolationModel.v so the thesis can
    reference both files together. *)
Load "IsolationModel".

(* ------------------------------------------------------------------ *)
(** ** §1  Overlap predicate                                            *)
(* ------------------------------------------------------------------ *)

(** [region_overlaps r1 r2] is the *negation* of [regions_disjoint].
    It directly corresponds to [hv_region_overlaps_pa] in stage2.c:
      end_a > base_b AND end_b > base_a. *)
Definition region_overlaps (r1 r2 : MemRegion) : Prop :=
  ~regions_disjoint r1 r2.

Lemma region_overlaps_iff :
  forall r1 r2 : MemRegion,
    region_overlaps r1 r2 <->
    r1.(pa_base) < r2.(pa_base) + r2.(pa_size) /\
    r2.(pa_base) < r1.(pa_base) + r1.(pa_size).
Proof.
  intros r1 r2.
  unfold region_overlaps, regions_disjoint.
  split.
  - intros H.
    apply Decidable.not_or in H.
    destruct H as [H1 H2].
    split; lia.
  - intros [H1 H2] [Hd | Hd]; lia.
Qed.

(* ------------------------------------------------------------------ *)
(** ** §2  Pre-condition for a safe mapping addition                    *)
(* ------------------------------------------------------------------ *)

(** [partition_map_ok s id new_region] holds when [new_region] does not
    overlap with any existing region belonging to a *different* partition.
    This is exactly the check performed in the cross-partition loop of
    [hv_stage2_map_partition] (stage2.c lines 121-133). *)
Definition partition_map_ok
    (s : HvState) (id : PartitionId) (new_region : MemRegion) : Prop :=
  forall (p : PartitionState) (r : MemRegion),
    In p s.(hv_partitions) ->
    p.(ps_id) <> id ->
    In r p.(ps_regions) ->
    regions_disjoint new_region r.

(* ------------------------------------------------------------------ *)
(** ** §3  State transition: add a region                               *)
(* ------------------------------------------------------------------ *)

(** Helper: update the region list of partition [id] inside a list of
    partition states. *)
Fixpoint add_region_to_partition
    (ps_list : list PartitionState)
    (id : PartitionId)
    (r : MemRegion)
    : list PartitionState :=
  match ps_list with
  | [] => []
  | p :: rest =>
      if Nat.eqb p.(ps_id) id
      then mkPartitionState p.(ps_id)
                            (r :: p.(ps_regions))
                            p.(ps_configured) :: rest
      else p :: add_region_to_partition rest id r
  end.

(** [add_region s id r] returns the new hypervisor state after appending
    region [r] to partition [id]'s region list.  It does not change the
    partition count or any other partition's state. *)
Definition add_region (s : HvState) (id : PartitionId) (r : MemRegion) : HvState :=
  mkHvState
    (add_region_to_partition s.(hv_partitions) id r)
    s.(hv_nr_partitions).

(* ------------------------------------------------------------------ *)
(** ** §4  Auxiliary lemmas about add_region_to_partition               *)
(* ------------------------------------------------------------------ *)

(** If [p] is in the updated list and has a different id, it is unchanged
    (same record, same regions). *)
Lemma add_region_other_partition_unchanged :
  forall (ps_list : list PartitionState) (id : PartitionId)
         (r : MemRegion) (p : PartitionState),
    In p (add_region_to_partition ps_list id r) ->
    p.(ps_id) <> id ->
    In p ps_list.
Proof.
  induction ps_list as [| hd tl IH]; simpl; intros id r p Hin Hne.
  - contradiction.
  - destruct (Nat.eqb_spec hd.(ps_id) id) as [Heq | Hne'].
    + (* head was updated *)
      simpl in Hin.
      destruct Hin as [Hhd | Htl].
      * (* p is the updated head *)
        subst p.
        simpl in Hne.
        exfalso; apply Hne; simpl.
        (* The updated head has the same id as hd *)
        exact Heq.
      * right; exact Htl.
    + (* head unchanged *)
      simpl in Hin.
      destruct Hin as [Hhd | Htl].
      * left; exact Hhd.
      * right; apply IH with (id := id) (r := r); assumption.
Qed.

(** If [p] is in the updated list and has id equal to [id], either it is
    the updated partition (whose regions are [r :: old_regions]) or it was
    already in the original list. *)
Lemma add_region_target_partition :
  forall (ps_list : list PartitionState) (id : PartitionId)
         (r : MemRegion) (p : PartitionState),
    In p (add_region_to_partition ps_list id r) ->
    p.(ps_id) = id ->
    exists (p_orig : PartitionState),
      In p_orig ps_list /\
      p_orig.(ps_id) = id /\
      p.(ps_regions) = r :: p_orig.(ps_regions) \/
      In p ps_list.
Proof.
  induction ps_list as [| hd tl IH]; simpl; intros id r p Hin Hid.
  - contradiction.
  - destruct (Nat.eqb_spec hd.(ps_id) id) as [Heq | Hne].
    + simpl in Hin.
      destruct Hin as [Hhd | Htl].
      * (* p is the updated head: p.ps_regions = r :: hd.ps_regions *)
        exists hd. left. split. left; reflexivity.
        split. exact Heq.
        subst p. simpl. reflexivity.
      * (* p is unchanged in the tail — witness with p itself *)
        exists p. right. right; exact Htl.
    + simpl in Hin.
      destruct Hin as [Hhd | Htl].
      * (* p = hd, which is already In (hd :: tl) *)
        exists p. right. left; exact Hhd.
      * (* p is in the recursively updated tail *)
        specialize (IH id r p Htl Hid).
        destruct IH as [p_orig Hcase].
        exists p_orig.
        destruct Hcase as [Hleft | Hright].
        -- left. destruct Hleft as [H1 [H2 H3]].
           split. right; exact H1. split; assumption.
        -- right. right; exact Hright.
Qed.

(* ------------------------------------------------------------------ *)
(** ** §4b  Additional structural lemma about add_region_to_partition   *)
(* ------------------------------------------------------------------ *)

(** Key lemma: if a partition [p] in the updated list has [p.ps_id = id],
    and region [r1] belongs to [p.ps_regions] but is NOT the newly added [r],
    then there is an original partition [p_orig] in the source list that also
    has [id] and contains [r1].

    This is the central structural fact needed to connect old regions of the
    updated partition back to the original state when applying [Hinv]. *)
Lemma add_region_old_region_in_orig :
  forall (ps_list : list PartitionState) (id : PartitionId)
         (r r1 : MemRegion) (p : PartitionState),
    In p (add_region_to_partition ps_list id r) ->
    p.(ps_id) = id ->
    In r1 p.(ps_regions) ->
    r1 <> r ->
    exists p_orig : PartitionState,
      In p_orig ps_list /\
      p_orig.(ps_id) = id /\
      In r1 p_orig.(ps_regions).
Proof.
  induction ps_list as [| hd tl IH]; simpl;
    intros id r r1 p Hin Hpid Hinr1 Hne.
  - (* empty list: In p [] is absurd *)
    contradiction.
  - destruct (Nat.eqb_spec hd.(ps_id) id) as [Hheq | Hhne].
    + (* hd was the updated partition: result is (updated_hd :: tl) *)
      simpl in Hin.
      destruct Hin as [Hhd | Htl].
      * (* p is the updated head: p.ps_regions = r :: hd.ps_regions *)
        subst p. simpl in Hinr1, Hpid.
        destruct Hinr1 as [Heq | Hold].
        -- (* r1 = r: contradicts r1 <> r — Heq : r = r1, Hne : r1 <> r *)
           exfalso; apply Hne; symmetry; exact Heq.
        -- (* r1 is in hd.ps_regions: take p_orig = hd *)
           exists hd. split.
           ++ left; reflexivity.
           ++ split; [exact Hheq | exact Hold].
      * (* p is unchanged in tl: p itself is the witness *)
        exists p. split.
        -- right; exact Htl.
        -- split; [exact Hpid | exact Hinr1].
    + (* hd was not updated: result is hd :: (recurse on tl) *)
      simpl in Hin.
      destruct Hin as [Hhd | Htl].
      * (* p = hd, but hd.ps_id <> id yet p.ps_id = id: contradiction *)
        subst p. simpl in Hpid. exfalso; apply Hhne; exact Hpid.
      * (* p is in the recursively updated tail *)
        destruct (IH id r r1 p Htl Hpid Hinr1 Hne) as [p_orig [Ho1 [Ho2 Ho3]]].
        exists p_orig. split.
        -- right; exact Ho1.
        -- split; assumption.
Qed.

(* ------------------------------------------------------------------ *)
(** ** §5  Key preservation theorem                                     *)
(* ------------------------------------------------------------------ *)

(** Decidable equality on [MemRegion].  Required to case-split on whether
    a candidate region [r1] equals the newly added region [r] or is older.
    Both fields are [nat], so equality is decidable via [Nat.eq_dec]. *)
Lemma mem_region_eq_dec : forall r1 r2 : MemRegion,
  {r1 = r2} + {r1 <> r2}.
Proof.
  intros r1 r2.
  destruct (Nat.eq_dec r1.(pa_base) r2.(pa_base)) as [Hb | Hb].
  - destruct (Nat.eq_dec r1.(pa_size) r2.(pa_size)) as [Hs | Hs].
    + left. destruct r1, r2; simpl in *; subst; reflexivity.
    + right. intros Heq. apply Hs. rewrite Heq. reflexivity.
  - right. intros Heq. apply Hb. rewrite Heq. reflexivity.
Qed.

(** Adding a region that passes [partition_map_ok] preserves the spatial
    isolation invariant.

    Proof strategy (four-case analysis on whether each of p1/p2 is the
    updated partition, and whether the disputed region is new or old):

    Case A  – p1.ps_id = id  (p1 was updated; p2 unchanged ⟹ p2 ∈ s).
      A1: r1 = r  → [partition_map_ok] directly gives disjointness.
      A2: r1 ≠ r  → [add_region_old_region_in_orig] yields p1_orig ∈ s
                     with r1; apply Hinv on (p1_orig, p2).

    Case B  – p1.ps_id ≠ id (p1 ∈ s unchanged).
      B1: p2.ps_id = id (p2 was updated).
        B1a: r2 = r  → [partition_map_ok] + [regions_disjoint_sym].
        B1b: r2 ≠ r  → [add_region_old_region_in_orig] yields p2_orig ∈ s;
                        apply Hinv on (p1, p2_orig).
      B2: p2.ps_id ≠ id → both partitions in s; apply Hinv directly.
*)
Theorem stage2_map_preserves_isolation :
  forall (s : HvState) (id : PartitionId) (r : MemRegion),
    spatial_isolation_invariant s ->
    partition_map_ok s id r ->
    spatial_isolation_invariant (add_region s id r).
Proof.
  intros s id r Hinv Hok.
  unfold spatial_isolation_invariant, add_region.
  simpl.
  intros p1 p2 Hin1 Hin2 Hne r1 r2 Hinr1 Hinr2.

  destruct (Nat.eqb_spec p1.(ps_id) id) as [Hid1 | Hid1'].

  (* ---------------------------------------------------------------- *)
  (* Case A: p1 is the updated partition (p1.ps_id = id)             *)
  (* ---------------------------------------------------------------- *)
  - assert (Hd1 : p2.(ps_id) <> id) by
      (intros Heq; apply Hne; rewrite Hid1, Heq; reflexivity).
    assert (Hin2_orig : In p2 s.(hv_partitions)).
    { apply add_region_other_partition_unchanged
        with (id := id) (r := r); assumption. }
    destruct (mem_region_eq_dec r1 r) as [Hr1_eq | Hr1_ne].

    + (* Case A1: r1 = r, the freshly added region *)
      subst r1.
      apply Hok with (p := p2); assumption.

    + (* Case A2: r1 ≠ r, so r1 is an old region already in s *)
      destruct (add_region_old_region_in_orig
                  s.(hv_partitions) id r r1 p1 Hin1 Hid1 Hinr1 Hr1_ne)
        as [p1_orig [Hinp1 [Hid1_orig Hinr1_orig]]].
      apply Hinv with (p1 := p1_orig) (p2 := p2).
      * exact Hinp1.
      * exact Hin2_orig.
      * rewrite Hid1_orig; intro H; exact (Hd1 (eq_sym H)).
      * exact Hinr1_orig.
      * exact Hinr2.

  (* ---------------------------------------------------------------- *)
  (* Case B: p1 is NOT the updated partition (p1 ∈ s unchanged)      *)
  (* ---------------------------------------------------------------- *)
  - assert (Hin1_orig : In p1 s.(hv_partitions)).
    { apply add_region_other_partition_unchanged
        with (id := id) (r := r); assumption. }
    destruct (Nat.eqb_spec p2.(ps_id) id) as [Hid2 | Hid2'].

    + (* Case B1: p2 is the updated partition *)
      destruct (mem_region_eq_dec r2 r) as [Hr2_eq | Hr2_ne].

      * (* Case B1a: r2 = r (new region) — symmetric to Case A1 *)
        subst r2.
        apply regions_disjoint_sym.
        apply Hok with (p := p1).
        -- exact Hin1_orig.
        -- intros Heq; apply Hne; rewrite Heq, Hid2; reflexivity.
        -- exact Hinr1.

      * (* Case B1b: r2 ≠ r, so r2 is an old region already in s *)
        destruct (add_region_old_region_in_orig
                    s.(hv_partitions) id r r2 p2 Hin2 Hid2 Hinr2 Hr2_ne)
          as [p2_orig [Hinp2 [Hid2_orig Hinr2_orig]]].
        apply Hinv with (p1 := p1) (p2 := p2_orig).
        -- exact Hin1_orig.
        -- exact Hinp2.
        -- rewrite Hid2_orig;
           intros Heq; apply Hne; rewrite Heq, Hid2; reflexivity.
        -- exact Hinr1.
        -- exact Hinr2_orig.

    + (* Case B2: neither p1 nor p2 is the updated partition *)
      assert (Hin2_orig : In p2 s.(hv_partitions)).
      { apply add_region_other_partition_unchanged
          with (id := id) (r := r); assumption. }
      apply Hinv with (p1 := p1) (p2 := p2); assumption.
Qed.

(* ------------------------------------------------------------------ *)
(** ** §6  Decidable mapping-ok check                                   *)
(* ------------------------------------------------------------------ *)

(** Boolean disjointness test, computable. *)
Definition regions_disjoint_b (r1 r2 : MemRegion) : bool :=
  Nat.leb (r1.(pa_base) + r1.(pa_size)) r2.(pa_base) ||
  Nat.leb (r2.(pa_base) + r2.(pa_size)) r1.(pa_base).

Lemma regions_disjoint_b_correct :
  forall r1 r2 : MemRegion,
    regions_disjoint_b r1 r2 = true <-> regions_disjoint r1 r2.
Proof.
  intros r1 r2.
  unfold regions_disjoint_b, regions_disjoint.
  rewrite Bool.orb_true_iff.
  rewrite Nat.leb_le.
  rewrite Nat.leb_le.
  reflexivity.
Qed.

(** Boolean check of [partition_map_ok]: iterate over all partitions and
    their regions, skipping the target partition. *)
Definition partition_map_ok_b
    (s : HvState) (id : PartitionId) (new_region : MemRegion) : bool :=
  forallb
    (fun p =>
       if Nat.eqb p.(ps_id) id then true
       else forallb (fun r => regions_disjoint_b new_region r) p.(ps_regions))
    s.(hv_partitions).

Lemma partition_map_ok_b_correct :
  forall (s : HvState) (id : PartitionId) (new_region : MemRegion),
    partition_map_ok_b s id new_region = true <->
    partition_map_ok s id new_region.
Proof.
  intros s id new_region.
  unfold partition_map_ok_b, partition_map_ok.
  rewrite forallb_forall.
  split.
  - intros Hb p r Hinp Hne Hinr.
    specialize (Hb p Hinp).
    destruct (Nat.eqb_spec p.(ps_id) id) as [Heq | Hne'].
    + exfalso; apply Hne; exact Heq.
    + rewrite forallb_forall in Hb.
      specialize (Hb r Hinr).
      apply regions_disjoint_b_correct; exact Hb.
  - intros Hprop p Hinp.
    destruct (Nat.eqb_spec p.(ps_id) id) as [Heq | Hne'].
    + reflexivity.
    + rewrite forallb_forall.
      intros r Hinr.
      apply regions_disjoint_b_correct.
      apply Hprop with (p := p); assumption.
Qed.

theory HavenIsolation
  imports Main
begin

(*
  HavenIsolation.thy
  Haven Hypervisor — Spatial Isolation in Isabelle/HOL

  This theory mirrors the core spatial isolation invariant from
  verification/coq/IsolationModel.v in Isabelle/HOL 2023 syntax.
  It is a second-opinion proof assistant artifact: the same policy
  claims checked by Coq are verified here using Isabelle's proof
  calculus (primarily auto/omega tactics).

  Corresponds to: src/core/mm/stage2.c, include/haven/stage2.h
  Chapter traceability: Thesis Ch. 4 §4.2 "Spatial Isolation Invariant"
                        Thesis Ch. 6 §6.1 "Dual-PA Verification"
*)

(* ------------------------------------------------------------------ *)
(* §1  Type aliases                                                     *)
(* ------------------------------------------------------------------ *)

type_synonym partition_id = nat
type_synonym phys_addr    = nat

(* ------------------------------------------------------------------ *)
(* §2  Memory region                                                    *)
(* ------------------------------------------------------------------ *)

(* A physical memory region is a half-open interval
   [pa_base, pa_base + pa_size).
   Corresponds to hv_mem_region.{pa_base, size} in stage2.h. *)
record mem_region =
  pa_base :: nat
  pa_size :: nat

(* ------------------------------------------------------------------ *)
(* §3  Core predicates                                                  *)
(* ------------------------------------------------------------------ *)

(* Two regions are disjoint when they share no physical address.
   Mirrors hv_range_overlaps (inverted) in stage2.c:
     end_a <= base_b  OR  end_b <= base_a *)
definition regions_disjoint :: "mem_region \<Rightarrow> mem_region \<Rightarrow> bool" where
  "regions_disjoint r1 r2 \<longleftrightarrow>
     pa_base r1 + pa_size r1 \<le> pa_base r2 \<or>
     pa_base r2 + pa_size r2 \<le> pa_base r1"

(* An address pa lies inside region r. *)
definition pa_in_region :: "nat \<Rightarrow> mem_region \<Rightarrow> bool" where
  "pa_in_region pa r \<longleftrightarrow>
     pa_base r \<le> pa \<and> pa < pa_base r + pa_size r"

(* ------------------------------------------------------------------ *)
(* §4  Spatial isolation invariant                                      *)
(* ------------------------------------------------------------------ *)

(* The partition map is a list of (partition_id, region_list) pairs.
   The invariant requires that any two regions owned by *different*
   partitions are disjoint.

   Corresponds to the cross-partition overlap guard in
   hv_stage2_map_partition (stage2.c lines 121-133). *)
definition spatial_isolation ::
    "(partition_id \<times> mem_region list) list \<Rightarrow> bool" where
  "spatial_isolation parts \<longleftrightarrow>
     (\<forall> p1 r1 p2 r2.
       (p1, r1) \<in> set parts \<longrightarrow>
       (p2, r2) \<in> set parts \<longrightarrow>
       p1 \<noteq> p2 \<longrightarrow>
       (\<forall> reg1 \<in> set r1. \<forall> reg2 \<in> set r2.
          regions_disjoint reg1 reg2))"

(* ------------------------------------------------------------------ *)
(* §5  Disjointness is symmetric                                        *)
(* ------------------------------------------------------------------ *)

lemma regions_disjoint_sym:
  "regions_disjoint r1 r2 \<Longrightarrow> regions_disjoint r2 r1"
  unfolding regions_disjoint_def
  by auto

(* ------------------------------------------------------------------ *)
(* §6  Primary theorem: disjoint regions cannot share an address        *)
(* ------------------------------------------------------------------ *)

(* If r1 and r2 are disjoint, and pa is inside r1, then pa is NOT
   inside r2.  This is the foundational lemma that underpins the thesis
   claim "a physical address can be owned by at most one partition." *)
theorem spatial_no_overlap:
  assumes "regions_disjoint r1 r2"
      and "pa_in_region pa r1"
  shows "\<not> pa_in_region pa r2"
proof -
  from assms(1) have disj:
    "pa_base r1 + pa_size r1 \<le> pa_base r2 \<or>
     pa_base r2 + pa_size r2 \<le> pa_base r1"
    unfolding regions_disjoint_def by simp
  from assms(2) have in1:
    "pa_base r1 \<le> pa \<and> pa < pa_base r1 + pa_size r1"
    unfolding pa_in_region_def by simp
  show "\<not> pa_in_region pa r2"
    unfolding pa_in_region_def
  proof
    assume in2: "pa_base r2 \<le> pa \<and> pa < pa_base r2 + pa_size r2"
    from disj in1 in2 show False by auto
  qed
qed

(* ------------------------------------------------------------------ *)
(* §7  Corollary: isolation invariant implies no address sharing        *)
(* ------------------------------------------------------------------ *)

(* If the global invariant holds and pa is contained in some region of
   partition p1, then pa is not contained in any region of distinct p2. *)
corollary isolation_no_shared_pa:
  assumes "spatial_isolation parts"
      and "(p1, regs1) \<in> set parts"
      and "(p2, regs2) \<in> set parts"
      and "p1 \<noteq> p2"
      and "reg1 \<in> set regs1"
      and "pa_in_region pa reg1"
  shows "\<forall> reg2 \<in> set regs2. \<not> pa_in_region pa reg2"
proof
  fix reg2
  assume "reg2 \<in> set regs2"
  from assms(1) have disj: "regions_disjoint reg1 reg2"
    unfolding spatial_isolation_def
    using assms(2) assms(3) assms(4) assms(5) \<open>reg2 \<in> set regs2\<close>
    by blast
  from spatial_no_overlap[OF disj assms(6)]
  show "\<not> pa_in_region pa reg2" .
qed

(* ------------------------------------------------------------------ *)
(* §8  Empty partition list satisfies the invariant                     *)
(* ------------------------------------------------------------------ *)

lemma empty_isolation: "spatial_isolation []"
  unfolding spatial_isolation_def
  by simp

(* ------------------------------------------------------------------ *)
(* §9  Adding a region preserves the invariant when no overlap exists   *)
(* ------------------------------------------------------------------ *)

(* Boolean overlap check for a single new region against an existing
   region — decidable version of \<not>regions_disjoint. *)
definition region_overlaps :: "mem_region \<Rightarrow> mem_region \<Rightarrow> bool" where
  "region_overlaps r1 r2 \<longleftrightarrow> \<not> regions_disjoint r1 r2"

(* The mapping precondition: new_r does not overlap with any region
   owned by a different partition.
   Mirrors the cross-partition guard in hv_stage2_map_partition. *)
definition partition_map_ok ::
    "(partition_id \<times> mem_region list) list \<Rightarrow>
     partition_id \<Rightarrow> mem_region \<Rightarrow> bool" where
  "partition_map_ok parts pid new_r \<longleftrightarrow>
     (\<forall> p regs.
       (p, regs) \<in> set parts \<longrightarrow>
       p \<noteq> pid \<longrightarrow>
       (\<forall> r \<in> set regs. regions_disjoint new_r r))"

(* add_region inserts new_r into partition pid's region list. *)
fun add_region ::
    "(partition_id \<times> mem_region list) list \<Rightarrow>
     partition_id \<Rightarrow> mem_region \<Rightarrow>
     (partition_id \<times> mem_region list) list" where
  "add_region [] _ _ = []"
| "add_region ((p, regs) # rest) pid r =
     (if p = pid
      then (p, r # regs) # rest
      else (p, regs) # add_region rest pid r)"

(* Key preservation lemma: a checked mapping addition preserves
   spatial_isolation.  This is the Isabelle mirror of the Coq theorem
   stage2_map_preserves_isolation in Stage2Policy.v. *)
lemma add_region_preserves_isolation:
  assumes "spatial_isolation parts"
      and "partition_map_ok parts pid new_r"
  shows "spatial_isolation (add_region parts pid new_r)"
  using assms
  unfolding spatial_isolation_def partition_map_ok_def
proof (induction parts)
  case Nil
  thus ?case by simp
next
  case (Cons hd tl)
  obtain hp hregs where hd_eq: "hd = (hp, hregs)" by (cases hd)
  show ?case
  proof (simp only: add_region.simps hd_eq, split if_split, intro conjI impI)
    (* Case: hp = pid — the head partition is being updated *)
    assume heq: "hp = pid"
    show "\<forall> p1 r1 p2 r2.
           (p1, r1) \<in> set ((hp, new_r # hregs) # tl) \<longrightarrow>
           (p2, r2) \<in> set ((hp, new_r # hregs) # tl) \<longrightarrow>
           p1 \<noteq> p2 \<longrightarrow>
           (\<forall> reg1 \<in> set r1. \<forall> reg2 \<in> set r2. regions_disjoint reg1 reg2)"
    proof (intro allI impI ballI)
      fix p1 r1 p2 r2 reg1 reg2
      assume hin1: "(p1, r1) \<in> set ((hp, new_r # hregs) # tl)"
      assume hin2: "(p2, r2) \<in> set ((hp, new_r # hregs) # tl)"
      assume hne:  "p1 \<noteq> p2"
      assume hreg1: "reg1 \<in> set r1"
      assume hreg2: "reg2 \<in> set r2"
      (* Derive the disjointness from the hypothesis *)
      from Cons.prems(1) Cons.prems(2) hin1 hin2 hne hreg1 hreg2 heq
      show "regions_disjoint reg1 reg2"
        apply (simp add: hd_eq)
        apply (erule disjE)
        (* p1 = hp case: reg1 may be new_r *)
        all: (auto simp add: regions_disjoint_def partition_map_ok_def
                              spatial_isolation_def)
        done
    qed
  next
    (* Case: hp \<noteq> pid — head is unchanged, recurse on tail *)
    assume hne: "hp \<noteq> pid"
    from Cons have ih: "spatial_isolation (add_region tl pid new_r)"
      by (auto simp add: spatial_isolation_def partition_map_ok_def hd_eq)
    thus "\<forall> p1 r1 p2 r2.
           (p1, r1) \<in> set ((hp, hregs) # add_region tl pid new_r) \<longrightarrow>
           (p2, r2) \<in> set ((hp, hregs) # add_region tl pid new_r) \<longrightarrow>
           p1 \<noteq> p2 \<longrightarrow>
           (\<forall> reg1 \<in> set r1. \<forall> reg2 \<in> set r2. regions_disjoint reg1 reg2)"
      using Cons.prems
      by (auto simp add: spatial_isolation_def partition_map_ok_def hd_eq)
  qed
qed

(* ------------------------------------------------------------------ *)
(* §10  Budget scheduler: temporal isolation                            *)
(* ------------------------------------------------------------------ *)

(* Minimal formalisation of the budget invariant.
   Full development is in verification/coq/BudgetScheduler.v.
   Corresponds to: src/core/sched/budget.c, hv_budget_set / hv_budget_consume. *)

record budget_state =
  bs_budget_ns   :: nat
  bs_period_ns   :: nat
  bs_consumed_ns :: nat

definition budget_valid :: "budget_state \<Rightarrow> bool" where
  "budget_valid b \<longleftrightarrow>
     bs_budget_ns b \<le> bs_period_ns b \<and>
     bs_consumed_ns b \<le> bs_budget_ns b"

(* Consuming time never violates the invariant (capping at budget_ns). *)
definition consume_budget :: "budget_state \<Rightarrow> nat \<Rightarrow> budget_state" where
  "consume_budget b amount =
     (let new_c = bs_consumed_ns b + amount in
      b \<lparr> bs_consumed_ns :=
            if new_c \<le> bs_budget_ns b then new_c
            else bs_budget_ns b \<rparr>)"

lemma consume_budget_preserves_valid:
  "budget_valid b \<Longrightarrow> budget_valid (consume_budget b amount)"
  unfolding budget_valid_def consume_budget_def
  by auto

(* set_budget establishes budget_valid from scratch. *)
definition set_budget :: "budget_state \<Rightarrow> nat \<Rightarrow> nat \<Rightarrow> budget_state" where
  "set_budget b new_budget new_period =
     b \<lparr> bs_budget_ns := new_budget,
         bs_period_ns  := new_period,
         bs_consumed_ns := 0 \<rparr>"

lemma set_budget_establishes_valid:
  "new_budget \<le> new_period \<Longrightarrow>
   budget_valid (set_budget b new_budget new_period)"
  unfolding budget_valid_def set_budget_def
  by auto

end

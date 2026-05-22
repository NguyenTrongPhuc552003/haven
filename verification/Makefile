# verification/Makefile
#
# Build and clean automation for all formal verification artefacts.
#
# All generated outputs are placed under $(REPO_ROOT)/build/verification/ so
# the source tree stays clean and .gitignore only needs one entry.
#
# Targets
# -------
#   all      (default) Compile Coq proofs + build Isabelle session
#   coq      Compile Coq proof files      → build/verification/coq/
#   isabelle Build Isabelle/HOL session   → build/verification/isabelle/
#   clean    Remove build/verification/
#   help     Print this message
#
# Usage
# -----
#   make -C verification            # build everything
#   make -C verification coq        # Coq only
#   make -C verification clean      # wipe generated files
#
# Environment
# -----------
#   COQC      Path to the coqc binary          (default: coqc)
#   ISABELLE  Path to the isabelle binary       (default: isabelle)
#
# Notes
# -----
# Coq sources are copied into the build directory before compilation so that
# relative 'Load "…"' references in Stage2Policy.v resolve correctly without
# polluting the source tree with .vo / .glob files.
#
# The Isabelle target is skipped gracefully when 'isabelle' is not on PATH;
# it is intended for local use and optional CI jobs only.

COQC     ?= coqc
ISABELLE ?= isabelle

# ── Directories ───────────────────────────────────────────────────────────────

REPO_ROOT := $(abspath ..)
BUILD_DIR := $(REPO_ROOT)/build/verification
COQ_BUILD := $(BUILD_DIR)/coq
ISA_BUILD := $(BUILD_DIR)/isabelle

COQ_SRC := coq
ISA_SRC := isabelle

# Coq sources listed in strict dependency order (IsolationModel must come first)
COQ_FILES := IsolationModel.v Stage2Policy.v BudgetScheduler.v

# ── Phony targets ─────────────────────────────────────────────────────────────

.PHONY: all coq isabelle clean help

all: coq isabelle

# ── Coq ───────────────────────────────────────────────────────────────────────
# Copy .v sources into COQ_BUILD, then compile in-place.
# A stamp file records a successful build so incremental rebuilds are fast.

coq: $(COQ_BUILD)/.coq_stamp

$(COQ_BUILD)/.coq_stamp: $(addprefix $(COQ_SRC)/,$(COQ_FILES)) | $(COQ_BUILD)
	@echo "[coq] copying sources → $(COQ_BUILD)"
	@cp $(addprefix $(COQ_SRC)/,$(COQ_FILES)) $(COQ_BUILD)/
	@echo "[coq] compiling IsolationModel.v"
	@cd $(COQ_BUILD) && $(COQC) IsolationModel.v
	@echo "[coq] compiling Stage2Policy.v"
	@cd $(COQ_BUILD) && $(COQC) Stage2Policy.v
	@echo "[coq] compiling BudgetScheduler.v"
	@cd $(COQ_BUILD) && $(COQC) BudgetScheduler.v
	@echo "[coq] all proofs verified"
	@touch $@

$(COQ_BUILD):
	@mkdir -p $@

# ── Isabelle ──────────────────────────────────────────────────────────────────
# Builds the HavenIsolation session.
# ISABELLE_OUTPUT redirects heap/image files into ISA_BUILD.
# The target is skipped with a warning when 'isabelle' is not available.

isabelle: $(ISA_BUILD)/.isabelle_stamp

$(ISA_BUILD)/.isabelle_stamp: $(ISA_SRC)/HavenIsolation.thy $(ISA_SRC)/ROOT | $(ISA_BUILD)
	@if ! command -v $(ISABELLE) >/dev/null 2>&1; then \
	  echo "[isabelle] WARNING: '$(ISABELLE)' not found on PATH - skipping"; \
	  touch $@; \
	else \
	  echo "[isabelle] building HavenIsolation session → $(ISA_BUILD)"; \
	  ISABELLE_OUTPUT=$(ISA_BUILD) $(ISABELLE) build -D $(ISA_SRC) && \
	  echo "[isabelle] session built" && \
	  touch $@; \
	fi

$(ISA_BUILD):
	@mkdir -p $@

# ── Housekeeping ──────────────────────────────────────────────────────────────

clean:
	@echo "[clean] removing $(BUILD_DIR)"
	@rm -rf $(BUILD_DIR)

help:
	@echo "Usage: make -C verification [target]"
	@echo ""
	@echo "  all      Compile Coq proofs and build Isabelle session (default)"
	@echo "  coq      Compile Coq proof files  →  build/verification/coq/"
	@echo "  isabelle Build Isabelle/HOL session →  build/verification/isabelle/"
	@echo "  clean    Remove build/verification/"
	@echo ""
	@echo "Environment variables:"
	@echo "  COQC=<path>      Override coqc binary (default: coqc)"
	@echo "  ISABELLE=<path>  Override isabelle binary (default: isabelle)"

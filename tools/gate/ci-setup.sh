#!/usr/bin/env bash
# Install the gate's tools at the versions pinned in quality/gate.json on an
# Ubuntu 24.04 CI runner, so CI and the owner's Mac report the same findings
# for the same code (one baseline for both). Usage: tools/gate/ci-setup.sh TOOL...
set -euo pipefail

CFG=quality/gate.json
pin() { python3 -c "import json,sys;print(json.load(open('$CFG'))['versions'][sys.argv[1]])" "$1"; }
BIN="$HOME/.local/bin"
mkdir -p "$BIN"
echo "$BIN" >>"${GITHUB_PATH:-/dev/null}"
export PATH="$BIN:$PATH"

for t in "$@"; do
	v=$(pin "$t")
	echo "ci-setup: $t $v"
	case "$t" in
	lizard | ruff)
		pipx install "$t==$v" >/dev/null ;;
	jscpd)
		npm install -g --silent "jscpd@$v" >/dev/null ;;
	gitleaks)
		# the release binary: a `go install` build reports no version
		curl -fsSL "https://github.com/gitleaks/gitleaks/releases/download/v$v/gitleaks_${v}_linux_x64.tar.gz" |
			tar -xz -C /tmp gitleaks && install /tmp/gitleaks "$BIN/" ;;
	shellcheck)
		curl -fsSL "https://github.com/koalaman/shellcheck/releases/download/v$v/shellcheck-v$v.linux.x86_64.tar.xz" |
			tar -xJ -C /tmp && install "/tmp/shellcheck-v$v/shellcheck" "$BIN/" ;;
	clang-tidy)
		major=${v%%.*}
		curl -fsSL https://apt.llvm.org/llvm.sh -o /tmp/llvm.sh
		sudo bash /tmp/llvm.sh "$major" >/dev/null
		sudo apt-get -o Acquire::Retries=5 install -y -qq "clang-tidy-$major" >/dev/null
		ln -sf "/usr/bin/clang-tidy-$major" "$BIN/clang-tidy" ;;
	cppcheck)
		if [ ! -x "$HOME/.cache/cppcheck-$v/bin/cppcheck" ]; then
			curl -fsSL "https://github.com/danmar/cppcheck/archive/refs/tags/$v.tar.gz" | tar -xz -C /tmp
			cmake -S "/tmp/cppcheck-$v" -B /tmp/cppcheck-build -G Ninja -DCMAKE_BUILD_TYPE=Release \
				-DCMAKE_INSTALL_PREFIX="$HOME/.cache/cppcheck-$v" -DUSE_MATCHCOMPILER=ON >/dev/null
			cmake --build /tmp/cppcheck-build >/dev/null
			cmake --install /tmp/cppcheck-build >/dev/null
		fi
		ln -sf "$HOME/.cache/cppcheck-$v/bin/cppcheck" "$BIN/cppcheck" ;;
	*)
		echo "ci-setup: unknown tool $t" >&2
		exit 2 ;;
	esac
done

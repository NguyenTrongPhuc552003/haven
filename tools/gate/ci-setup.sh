#!/usr/bin/env bash
# Install the gate's tools at the versions pinned in quality/gate.json on an
# Ubuntu 24.04 CI runner, so CI and the owner's Mac report the same findings
# for the same code (one baseline for both). Run from the repository whose
# profile is used. Usage: tools/gate/ci-setup.sh [TOOL...]; without a tool,
# every tool the profile pins. The checkpatch step also works on macOS.
set -euo pipefail

CFG=quality/gate.json
pin() { python3 -c "import json,sys;print(json.load(open('$CFG'))['versions'][sys.argv[1]])" "$1"; }
if [ $# -eq 0 ]; then
	# shellcheck disable=SC2046
	set -- $(python3 -c "import json;c=json.load(open('$CFG'));print(' '.join(list(c['versions']) + (['checkpatch'] if 'checkpatch' in c else [])))")
fi
BIN="$HOME/.local/bin"
mkdir -p "$BIN"
echo "$BIN" >>"${GITHUB_PATH:-/dev/null}"
export PATH="$BIN:$PATH"

checkpatch_install() {
	# scripts/checkpatch.pl and its two data files from the pinned Linux tag,
	# each checked against the profile's SHA-256
	python3 - "$CFG" <<'PY'
import hashlib, json, os, sys, urllib.request
c = json.load(open(sys.argv[1]))["checkpatch"]
d = os.path.expanduser(c["dir"])
os.makedirs(d, exist_ok=True)
for name, want in c["sha256"].items():
    url = f"https://raw.githubusercontent.com/torvalds/linux/{c['tag']}/scripts/{name}"
    data = urllib.request.urlopen(url, timeout=60).read()
    got = hashlib.sha256(data).hexdigest()
    if got != want:
        sys.exit(f"ci-setup: {name} from {c['tag']}: SHA-256 {got}, pinned {want}")
    with open(os.path.join(d, name), "wb") as f:
        f.write(data)
print(f"ci-setup: checkpatch {c['tag']} in {d}")
PY
}

for t in "$@"; do
	if [ "$t" = checkpatch ]; then
		checkpatch_install
		continue
	fi
	v=$(pin "$t")
	echo "ci-setup: $t $v"
	case "$t" in
	go)
		# the pinned toolchain, ahead of the runner's own
		curl -fsSL "https://go.dev/dl/go$v.linux-amd64.tar.gz" | tar -xz -C "$HOME/.local"
		ln -sf "$HOME/.local/go/bin/go" "$HOME/.local/go/bin/gofmt" "$BIN/" ;;
	golangci-lint)
		curl -fsSL "https://github.com/golangci/golangci-lint/releases/download/v$v/golangci-lint-$v-linux-amd64.tar.gz" |
			tar -xz -C /tmp && install "/tmp/golangci-lint-$v-linux-amd64/golangci-lint" "$BIN/" ;;
	govulncheck)
		GOBIN="$BIN" "$BIN/go" install "golang.org/x/vuln/cmd/govulncheck@v$v" ;;
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

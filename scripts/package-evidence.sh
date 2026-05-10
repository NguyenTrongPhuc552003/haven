#!/usr/bin/env sh
set -eu

./scripts/ci-preflight.sh
./scripts/ci-metadata.sh

mkdir -p build/evidence
cp -r docs/methodology build/evidence/methodology
cp -r docs/porting build/evidence/porting
cp build/ci/metadata.txt build/evidence/metadata.txt

mkdir -p build/evidence/imx95/logs
mkdir -p build/evidence/imx95/metrics
mkdir -p build/evidence/imx95/captures

cat > build/evidence/imx95/README.md << 'EOF'
# i.MX95 Evidence Bundle

Place campaign artifacts here:
- logs/: boot and runtime logs
- metrics/: latency and deadline metrics
- captures/: serial captures and supplementary traces
EOF

cp docs/methodology/IMX95_EVIDENCE_TEMPLATE.md build/evidence/imx95/evidence-template.md

echo "[evidence] package created at build/evidence"

# Vercel Deployment Guide

This repository includes a static thesis portal under site/ and vercel.json.

## Deployment modes

1. Dashboard-connected deployment:
- Import repository in Vercel.
- Configure project root as repository root.
- Keep output directory as site.

2. CLI deployment:
- vercel login
- vercel --prod

## Recommended environment

1. Production branch: main.
2. Preview deployments: pull requests.
3. Keep deployment focused on thesis portal and documentation entrypoint.

## Optional GitHub automation

Use GitHub Actions with these secrets:
1. VERCEL_TOKEN
2. VERCEL_ORG_ID
3. VERCEL_PROJECT_ID

## Validation checklist

1. Root URL renders site/index.html.
2. Route rewrite keeps deep links on index.html.
3. Published page links back to repository and roadmap docs.

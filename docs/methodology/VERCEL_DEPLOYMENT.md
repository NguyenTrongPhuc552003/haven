# Vercel Deployment Guide

The documentation portal is an **Astro Starlight** site located under `website/`.
It is deployed to `https://haven-tau-eight.vercel.app/`.

## Repository layout

```
vercel.json          ← root config: rootDirectory = website, framework = astro
website/
  astro.config.mjs   ← Starlight config, sidebar, site URL
  package.json       ← haven-docs, astro ^5, @astrojs/starlight ^0.31
  src/
    content/docs/    ← 23+ MDX pages (index, getting-started, architecture,
                       platform, thesis, reference)
    styles/          ← custom Haven-blue CSS
    assets/          ← logo SVGs
```

## Root vercel.json

```json
{
  "buildCommand": "npm run build",
  "outputDirectory": "dist",
  "installCommand": "npm install",
  "framework": "astro",
  "rootDirectory": "website"
}
```

Vercel reads this file, changes into `website/`, runs `npm install && npm run build`,
and serves the `dist/` output.

## Deployment modes

### 1. Git-push (recommended)

Push to `main`. Vercel automatically re-deploys.
GitHub Actions also runs the `website-build` job to catch build failures early.

### 2. CLI

```sh
cd /path/to/haven
vercel --prod
```

The CLI reads `vercel.json` at the repo root and builds from `website/`.

## GitHub Actions integration

`.github/workflows/ci.yml` contains a `website-build` job:

```yaml
website-build:
  runs-on: ubuntu-latest
  defaults:
    run:
      working-directory: website
  steps:
    - uses: actions/checkout@v4
    - uses: actions/setup-node@v4
      with:
        node-version: "20"
        cache: "npm"
        cache-dependency-path: website/package-lock.json
    - run: npm install
    - run: npm run build
```

Optional Vercel deploy workflow: `.github/workflows/vercel.yml` uses
`VERCEL_TOKEN`, `VERCEL_ORG_ID`, `VERCEL_PROJECT_ID` secrets for automated
production deployments from CI.

## Local development

```sh
cd website
npm install
npm run dev        # hot-reload dev server at http://localhost:4321
npm run build      # production build into dist/
npm run preview    # preview the built site
```

## Validation checklist

1. `npm run build` in `website/` completes with 0 errors.
2. Root URL renders the Starlight splash page with Haven title.
3. Sidebar navigation covers all 5 sections.
4. "Background & Related Work" page renders with Omnivisor citation.
5. GitHub link in social header points to the correct repository.

## Content sections

| URL prefix | Description |
|---|---|
| `/getting-started/` | Introduction, quick-start, repository structure |
| `/architecture/` | Isolation model, Stage-2 MMU, IRQ, SMMU, budget scheduler |
| `/platform/` | i.MX95, QEMU arm64, porting guide |
| `/thesis/` | Background, traceability, evaluation, milestones, evidence |
| `/reference/` | API, FAQ, changelog, contributing, security |

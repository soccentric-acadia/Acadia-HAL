# Task 011 — Server module registry deployment (VB-4) — Acadia-HAL scope (validation-only)

Parent task: [`Acadia/tasks/011-server-module-registry-deployment/plan.md`](https://github.com/soccentric-acadia/Acadia/blob/develop/tasks/011-server-module-registry-deployment/plan.md)

## Scope in this repository

Validation-only. No code change. `protos/registry/modules.yaml` at this
repo's `7184d94` (`master`) was vendored (manually copied, not generated)
into `Acadia-Server` as that repo's missing module registry.

## Role in cross-repository validation

This repo is the source of truth the vendored copy must be kept in sync
with. `integration-ivv.md` V-G04 ("single module registry source... generate
all from one HAL rev in CI") already tracks replacing this manual vendoring
with a real generation pipeline; until then, anyone changing this file
should know `Acadia-Server` carries a manual snapshot that needs a matching
manual update.

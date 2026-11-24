#!/usr/bin/env python3
###############################################################################
# Copyright (c) 2022-25, Lawrence Livermore National Security, LLC and RADIUSS
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
###############################################################################
"""Collect per-job CI section timings and render trend charts.

Intended to run in the parent pipeline's ``pages`` job, after the per-machine
child pipelines complete. It:

  1. Gathers every child job's ``timings.json`` (written by
     ``scripts/gitlab/build_and_test.sh``) via the GitLab API.
  2. Merges those records into an accumulating ``history.jsonl`` pulled from the
     latest default-branch pipeline's artifact, so the series survives the
     per-pipeline artifact expiry window.
  3. Renders one cumulative (stacked-area) chart per ``(machine, job)`` — commit
     on the X axis, seconds on the Y axis, one band per top-level section — plus
     an ``index.html`` linking them, under ``public/``.

Offline / local testing
------------------------
Set ``TIMINGS_OFFLINE=1`` and place ``timings.json`` files (any names) under
``TIMINGS_INPUT_DIR`` (default ``./timings_input``). A local ``history.jsonl``,
if present in the working directory, is used as the starting history. No network
calls are made in this mode, so the chart rendering can be exercised without a
GitLab instance.
"""

import glob
import json
import os
import sys
import urllib.error
import urllib.parse
import urllib.request

OUTPUT_DIR = "public"
HISTORY_FILE = "history.jsonl"
# Cap the retained history to the most recent commits to keep the dataset and
# charts manageable. Dropped commits are reported on stdout.
MAX_COMMITS = 200

API_URL = os.environ.get("CI_API_V4_URL", "")
PROJECT_ID = os.environ.get("CI_PROJECT_ID", "")
PIPELINE_ID = os.environ.get("CI_PIPELINE_ID", "")
JOB_TOKEN = os.environ.get("CI_JOB_TOKEN", "")
DEFAULT_BRANCH = os.environ.get("CI_DEFAULT_BRANCH", "develop")
PAGES_JOB_NAME = os.environ.get("CI_JOB_NAME", "pages")
OFFLINE = os.environ.get("TIMINGS_OFFLINE", "") == "1"
INPUT_DIR = os.environ.get("TIMINGS_INPUT_DIR", "timings_input")


def log(msg):
    print("[timings] {}".format(msg), flush=True)


###############################################################################
# GitLab API helpers
###############################################################################

def _api_request(url):
    """GET a URL with the CI job token. Returns response bytes, or None on 404."""
    req = urllib.request.Request(url, headers={"JOB-TOKEN": JOB_TOKEN})
    try:
        with urllib.request.urlopen(req) as resp:
            return resp.read()
    except urllib.error.HTTPError as err:
        if err.code == 404:
            return None
        raise


def _api_get_json_paginated(path):
    """GET a paginated API list endpoint, returning the concatenated JSON list."""
    results = []
    page = 1
    while True:
        sep = "&" if "?" in path else "?"
        url = "{}/{}{}per_page=100&page={}".format(API_URL, path, sep, page)
        body = _api_request(url)
        if not body:
            break
        chunk = json.loads(body)
        if not chunk:
            break
        results.extend(chunk)
        if len(chunk) < 100:
            break
        page += 1
    return results


def gather_pipeline_timings():
    """Download every child job's timings.json for the current pipeline."""
    records = []
    bridges = _api_get_json_paginated(
        "projects/{}/pipelines/{}/bridges".format(PROJECT_ID, PIPELINE_ID))
    downstream_ids = [
        b["downstream_pipeline"]["id"]
        for b in bridges
        if b.get("downstream_pipeline")
    ]
    log("Found {} downstream child pipeline(s)".format(len(downstream_ids)))

    for dp_id in downstream_ids:
        jobs = _api_get_json_paginated(
            "projects/{}/pipelines/{}/jobs".format(PROJECT_ID, dp_id))
        for job in jobs:
            artifact_url = "{}/projects/{}/jobs/{}/artifacts/timings.json".format(
                API_URL, PROJECT_ID, job["id"])
            body = _api_request(artifact_url)
            if not body:
                continue
            try:
                records.append(json.loads(body))
            except json.JSONDecodeError:
                log("Skipping malformed timings.json from job {}".format(job["id"]))
    log("Collected {} timings record(s) from this pipeline".format(len(records)))
    return records


def load_remote_history():
    """Fetch history.jsonl from the latest default-branch pipeline artifact."""
    path = "projects/{}/jobs/artifacts/{}/raw/{}".format(
        PROJECT_ID, urllib.parse.quote(DEFAULT_BRANCH, safe=""), HISTORY_FILE)
    url = "{}/{}?job={}".format(API_URL, path, urllib.parse.quote(PAGES_JOB_NAME))
    body = _api_request(url)
    if not body:
        log("No prior history found (first run on {}?)".format(DEFAULT_BRANCH))
        return []
    return parse_history_bytes(body)


###############################################################################
# Offline / local input
###############################################################################

def gather_local_timings():
    records = []
    for path in sorted(glob.glob(os.path.join(INPUT_DIR, "*.json"))):
        with open(path) as handle:
            records.append(json.load(handle))
    log("Loaded {} local timings record(s) from {}".format(len(records), INPUT_DIR))
    return records


def load_local_history():
    if not os.path.exists(HISTORY_FILE):
        return []
    with open(HISTORY_FILE, "rb") as handle:
        return parse_history_bytes(handle.read())


###############################################################################
# Merge & persist
###############################################################################

def parse_history_bytes(body):
    records = []
    for line in body.decode("utf-8").splitlines():
        line = line.strip()
        if line:
            records.append(json.loads(line))
    return records


def record_key(record):
    return (record.get("sha", ""), record.get("machine", ""), record.get("job", ""))


def merge_records(history, new_records):
    """Merge new records into history, deduped by (sha, machine, job).

    Newer records (higher ts) win on conflict. History is then trimmed to the
    most recent MAX_COMMITS distinct commits.
    """
    merged = {}
    for record in history + new_records:
        key = record_key(record)
        existing = merged.get(key)
        if existing is None or record.get("ts", 0) >= existing.get("ts", 0):
            merged[key] = record

    records = list(merged.values())

    # Keep only the most recent MAX_COMMITS commits (by latest ts per sha).
    latest_ts_by_sha = {}
    for record in records:
        sha = record.get("sha", "")
        latest_ts_by_sha[sha] = max(latest_ts_by_sha.get(sha, 0), record.get("ts", 0))
    kept_shas = set(
        sha for sha, _ in
        sorted(latest_ts_by_sha.items(), key=lambda kv: kv[1], reverse=True)[:MAX_COMMITS]
    )
    dropped = len(latest_ts_by_sha) - len(kept_shas)
    if dropped > 0:
        log("Trimming history: dropping {} oldest commit(s) beyond MAX_COMMITS={}"
            .format(dropped, MAX_COMMITS))
    records = [r for r in records if r.get("sha", "") in kept_shas]

    records.sort(key=lambda r: (r.get("ts", 0), r.get("machine", ""), r.get("job", "")))
    return records


def write_history(records):
    with open(HISTORY_FILE, "w") as handle:
        for record in records:
            handle.write(json.dumps(record, sort_keys=True) + "\n")
    log("Wrote {} record(s) to {}".format(len(records), HISTORY_FILE))


###############################################################################
# Charts
###############################################################################

def top_level_section_order(records):
    """Stable ordering of depth-0 section names, by first appearance over time."""
    order = []
    seen = set()
    for record in sorted(records, key=lambda r: r.get("ts", 0)):
        for section in record.get("sections", []):
            if section.get("depth", 0) != 0:
                continue
            name = section["name"]
            if name not in seen:
                seen.add(name)
                order.append(name)
    return order


def render_charts(records):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    # Group records per (machine, job).
    groups = {}
    for record in records:
        groups.setdefault((record.get("machine", ""), record.get("job", "")), []).append(record)

    os.makedirs(OUTPUT_DIR, exist_ok=True)
    chart_index = {}  # machine -> list of (job, relative_png_path)

    for (machine, job), group in sorted(groups.items()):
        group.sort(key=lambda r: r.get("ts", 0))
        section_names = top_level_section_order(group)
        if not section_names:
            continue

        labels = [r.get("short_sha") or r.get("sha", "")[:8] for r in group]
        x = list(range(len(group)))
        series = []
        for name in section_names:
            row = []
            for record in group:
                seconds = 0
                for section in record.get("sections", []):
                    if section.get("depth", 0) == 0 and section["name"] == name:
                        seconds = section.get("seconds", 0)
                        break
                row.append(seconds)
            series.append(row)

        fig, ax = plt.subplots(figsize=(max(8, len(group) * 0.35), 6))
        ax.stackplot(x, series, labels=section_names)
        ax.set_title("{} / {} — section timings".format(machine, job))
        ax.set_xlabel("commit")
        ax.set_ylabel("seconds")
        ax.set_xticks(x)
        ax.set_xticklabels(labels, rotation=90, fontsize=7)
        ax.legend(loc="upper left", fontsize=7, ncol=2)
        ax.margins(x=0)
        fig.tight_layout()

        machine_dir = os.path.join(OUTPUT_DIR, machine or "unknown")
        os.makedirs(machine_dir, exist_ok=True)
        rel_path = os.path.join(machine or "unknown", "{}.png".format(job or "unknown"))
        fig.savefig(os.path.join(OUTPUT_DIR, rel_path), dpi=100)
        plt.close(fig)
        chart_index.setdefault(machine or "unknown", []).append((job or "unknown", rel_path))

    write_index(chart_index)


def write_index(chart_index):
    parts = [
        "<!DOCTYPE html>",
        "<html><head><meta charset='utf-8'>",
        "<title>Umpire CI timing trends</title>",
        "<style>body{font-family:sans-serif;margin:2rem;}"
        "h2{margin-top:2rem;}img{max-width:100%;border:1px solid #ccc;margin:.5rem 0;}"
        "</style></head><body>",
        "<h1>Umpire CI timing trends</h1>",
        "<p>Cumulative per-section build/test timings over commits, by machine and job.</p>",
    ]
    for machine in sorted(chart_index):
        parts.append("<h2>{}</h2>".format(machine))
        for job, rel_path in sorted(chart_index[machine]):
            parts.append("<h3>{}</h3>".format(job))
            parts.append("<img src='{}' alt='{} {}'>".format(rel_path, machine, job))
    parts.append("</body></html>")
    with open(os.path.join(OUTPUT_DIR, "index.html"), "w") as handle:
        handle.write("\n".join(parts))
    log("Wrote {}/index.html".format(OUTPUT_DIR))


###############################################################################
# Main
###############################################################################

def main():
    if OFFLINE:
        log("Running in OFFLINE mode")
        new_records = gather_local_timings()
        history = load_local_history()
    else:
        new_records = gather_pipeline_timings()
        history = load_remote_history()

    merged = merge_records(history, new_records)
    if not merged:
        log("No timing records available; nothing to chart.")
        # Still emit history (empty) and an index so the pages job succeeds.
        write_history(merged)
        os.makedirs(OUTPUT_DIR, exist_ok=True)
        write_index({})
        return 0

    write_history(merged)
    render_charts(merged)
    return 0


if __name__ == "__main__":
    sys.exit(main())

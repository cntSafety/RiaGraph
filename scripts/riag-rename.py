#!/usr/bin/env python3
"""Mechanical ladybug -> RiaGraph token rename.

This script is the single implementation of the substitution table in
``docs/riagraph-fork-plan.md`` section 3.1.  It is kept in the tree permanently
because it is also what makes upstream cherry-picks work: ``--patch`` runs a
unified diff produced against ``ladybugdb/ladybug`` through exactly the same
rules, so an upstream commit can be transplanted onto the renamed tree.

Usage
-----
    python scripts/riag-rename.py                 # rewrite the working tree
    python scripts/riag-rename.py --dry-run       # show what would change
    python scripts/riag-rename.py --check         # exit 1 if a token remains
    python scripts/riag-rename.py --report        # list ladybugdb occurrences
    python scripts/riag-rename.py --patch up.diff # transform a unified diff

Rules that are deliberately *not* automated (plan sections 3.1, 3.4, 3.8):

* ``ladybugdb`` / ``LadybugDB`` mean three different things (a GitHub org, an
  npm scope, a Java group).  They are reported, never rewritten.
* ``lbdb`` is context dependent (``~/.lbdb/``, ``catalog.lbdb``, the ``.lbdb``
  database suffix).  It is left alone.
* Lines carrying a ``riag-rename:skip`` marker are passed through untouched.
  This is how provenance survives the rename: upstream issue and pull-request
  URLs (``LadybugDB/ladybug#611``), third-party repositories that happen to
  carry a token (``Ladybug-Memory/icebug-format``, ``fabubaker/lbug``), and
  the PyPI package names ``migrate-riag-db.py`` installs to read old
  databases.  Rewriting any of those produces a link to a repository that
  never existed.  Plan section 3.8 protects the equivalent ``kuzudb/kuzu``
  URLs; these are the same category.
"""

import argparse
import os
import re
import subprocess
import sys

# The repository being renamed. --root points this at a submodule; the bindings live
# in their own repositories but share this one implementation.
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ---------------------------------------------------------------------------
# Token table (plan section 3.1)
# ---------------------------------------------------------------------------
# Order matters: LadybugDB before Ladybug before ladybug.  A single ordered
# alternation gives us that for free -- Python's `re` tries alternatives
# left-to-right at each position, so the longest/most-specific spelling wins
# before a shorter prefix of it can match.
#
# `lbug` is not a substring of `ladybug`, so the two families never interact.

REPORT_ONLY = ("LadybugDB", "ladybugdb")

REPLACEMENTS = (
    ("Ladybug", "RiaGraph"),
    ("LADYBUG", "RIAG"),
    ("ladybug", "riagraph"),
    ("Lbug", "RiaG"),
    ("LBUG", "RIAG"),
    ("lbug", "riag"),
)

TOKEN_RE = re.compile("|".join(REPORT_ONLY + tuple(a for a, _ in REPLACEMENTS)))

SUBSTITUTIONS = dict(REPLACEMENTS)

SKIP_MARKER = "riag-rename:skip"
# For formats with no trailing-comment syntax -- the .test DSL, where the token has to
# sit inside a directive line -- put the marker on the line above instead.
SKIP_NEXT_MARKER = "riag-rename:skip-next"

# ---------------------------------------------------------------------------
# Scope (plan section 5.1 item 6 and section 3.8)
# ---------------------------------------------------------------------------

EXCLUDED_PREFIXES = (
    ".git/",
    "dataset/",
    "test/answers/",
)

EXCLUDED_PATHS = (
    # Random fixture data: case-insensitive greps produce false positives.
    # The whole directory is excluded above; this list is for single files.
    "LICENSE",  # Kuzu Inc. / Ladybug Memory Inc. lines are required by the MIT terms.
    "docs/riagraph-fork-plan.md",  # The plan quotes both the before and after spellings.
    "docs/riagraph-open-infra.md",
    "docs/upstream-integration-plan-2026-10.md",  # Quotes upstream names and SHAs throughout.
    "docs/upstream-picks.md",
    "scripts/riag-rename.py",  # This file quotes both spellings by necessity.
    # Asserts the old spellings are *gone* -- it has to name them to look for them,
    # and it writes an LBUG-magic database fixture to prove the legacy read path works.
    "docker/build-env/verify.sh",
)

# Paths whose new name does not follow from the token table (plan section 3.7).
RENAME_OVERRIDES = {
    "logo/lbug-logo.png": "logo/riagraph-logo.png",
}

# ---------------------------------------------------------------------------
# Profiles (plan section 3.6)
# ---------------------------------------------------------------------------
# The token table alone gets the language bindings wrong in a handful of places,
# because one spelling means two things.  `ladybug` is the Python *distribution*
# name (-> riagraph) and also the *import* name (-> riag); `lbug` is an
# identifier prefix (-> riag) but sits inside the Java package `com.lbugdb`
# (-> com.riagraph).  And `ladybugdb` is report-only in the token table
# precisely because it is a GitHub org here, an npm scope there, and a Maven
# group somewhere else.
#
# Each profile lists the longer, more specific spellings, applied in order
# BEFORE the token table.  Anything a profile does not claim falls through to
# the ordinary rules, so these tables stay small and reviewable.

_GITHUB_ORG = "cntSafety"

# Every binding names the core repository, the shared dataset repository and its own
# repository.  Longest first: `ladybug-python` has to be claimed before `ladybug`.
def _repo_urls(binding_repo, upstream_repo):
    rules = []
    for org in ("LadybugDB", "ladybugdb"):
        rules.append(("%s/%s" % (org, upstream_repo), "%s/%s" % (_GITHUB_ORG, binding_repo)))
        rules.append(("%s/dataset" % org, "%s/RiaGraphDataset" % _GITHUB_ORG))
        rules.append(("%s/ladybug" % org, "%s/RiaGraph" % _GITHUB_ORG))
    return rules


PROFILES = {
    # The core repository: the token table is already correct for it.
    "core": [],

    "python": _repo_urls("RiaGraphPython", "ladybug-python") + [
        # Import name, not the distribution name.  `riagraph` is what users
        # `pip install`; `riag` is what they `import`.
        ("import ladybug", "import riag"),
        ("from ladybug", "from riag"),
        ("ladybug._lbug", "riag._lbug"),
        ('packages = ["ladybug"]', 'packages = ["riag"]'),
        ('ladybug = "src_py"', 'riag = "src_py"'),
        ('ladybug = ["py.typed"]', 'riag = ["py.typed"]'),
        ("build/ladybug", "build/riag"),
    ],

    "nodejs": _repo_urls("RiaGraphNodejs", "ladybug-nodejs") + [
        ("@ladybugdb/", "@riagraph/"),
    ],

    "java": _repo_urls("RiaGraphJava", "ladybug-java") + [
        # The Java package, the Maven group and the Android example's application id
        # all collapse to com.riagraph. Upstream is inconsistent here -- the `package`
        # declarations say com.ladybugdb while the directories are com/lbugdb -- so
        # both spellings are listed, and the rename incidentally lines them up.
        ("com.lbugdb", "com.riagraph"),
        ("com/lbugdb", "com/riagraph"),
        ("com.ladybugdb", "com.riagraph"),
        ("com/ladybugdb", "com/riagraph"),
        # JNI encodes the package in the exported symbol name with underscores. These
        # have to track the package exactly or the native methods fail to bind at
        # runtime, which no compiler will catch.
        ("com_lbugdb", "com_riagraph"),
        ("com_ladybugdb", "com_riagraph"),
    ],

    "rust": _repo_urls("RiaGraphRust", "ladybug-rust") + [],

    "wasm": _repo_urls("RiaGraphWasm", "ladybug-wasm") + [
        ("@ladybugdb/", "@riagraph/"),
    ],

    "extension": _repo_urls("RiaGraphExtension", "extensions") + [],
}


def in_scope(path):
    """True if `path` may be rewritten."""
    normalised = path.replace("\\", "/")
    if normalised in EXCLUDED_PATHS:
        return False
    for prefix in EXCLUDED_PREFIXES:
        if normalised.startswith(prefix):
            return False
    return True


# ---------------------------------------------------------------------------
# Text transformation
# ---------------------------------------------------------------------------


def substitute(text, profile=()):
    """Apply the token table to `text`, honouring per-line skip markers.

    `profile` is an ordered sequence of (old, new) pairs applied before the
    token table, for the places where one spelling means two things -- see
    PROFILES.

    Returns (new_text, n_replaced, report) where `report` maps a report-only
    token to the number of times it was seen and left alone.
    """
    replaced = 0
    report = {}

    def replace_token(match):
        nonlocal replaced
        token = match.group(0)
        if token in SUBSTITUTIONS:
            replaced += 1
            return SUBSTITUTIONS[token]
        report[token] = report.get(token, 0) + 1
        return token

    out_lines = []
    skip_this_line = False
    # keepends so line terminators survive untouched; CRLF files stay CRLF.
    for line in text.splitlines(True):
        skip = skip_this_line or SKIP_MARKER in line
        # `skip-next` contains `skip`, so it is already covered for its own line.
        skip_this_line = SKIP_NEXT_MARKER in line
        if skip:
            out_lines.append(line)
            continue
        for old, new in profile:
            if old in line:
                replaced += line.count(old)
                line = line.replace(old, new)
        out_lines.append(TOKEN_RE.sub(replace_token, line))
    return "".join(out_lines), replaced, report


def rename_path(path, profile=()):
    """Return the renamed path for `path`, or None if the name is unchanged."""
    normalised = path.replace("\\", "/")
    if normalised in RENAME_OVERRIDES:
        return RENAME_OVERRIDES[normalised]
    new, count, _ = substitute(normalised, profile)
    return new if count else None


# ---------------------------------------------------------------------------
# File access
# ---------------------------------------------------------------------------


def tracked_files():
    """Every regular file git tracks, as repo-relative POSIX paths.

    Submodule gitlinks (mode 160000) and symlinks (120000) are dropped: a
    submodule is renamed by its own run of this script inside its own
    repository, and a symlink has no content of ours to rewrite.
    """
    out = subprocess.check_output(["git", "ls-files", "-z", "--stage"], cwd=REPO_ROOT)
    paths = []
    for entry in out.split(b"\0"):
        if not entry:
            continue
        meta, _, path = entry.partition(b"\t")
        if meta.split(b" ", 1)[0] in (b"160000", b"120000"):
            continue
        paths.append(path.decode("utf-8"))
    return paths


def is_binary(blob):
    """Content sniffing: a NUL byte in the first 8 KiB means binary."""
    return b"\0" in blob[:8192]


def read(path):
    with open(os.path.join(REPO_ROOT, path), "rb") as handle:
        return handle.read()


def write(path, blob):
    with open(os.path.join(REPO_ROOT, path), "wb") as handle:
        handle.write(blob)


def git(*args):
    subprocess.check_call(["git"] + list(args), cwd=REPO_ROOT)


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------


def run_rename(dry_run, profile=()):
    content_changed = []
    renames = []
    report = {}
    skipped_binary = 0

    for path in tracked_files():
        if not in_scope(path):
            continue

        new_path = rename_path(path, profile)
        if new_path:
            renames.append((path, new_path))

        blob = read(path)
        if is_binary(blob):
            skipped_binary += 1
            continue

        try:
            text = blob.decode("utf-8")
        except UnicodeDecodeError:
            skipped_binary += 1
            continue

        new_text, count, file_report = substitute(text, profile)
        for token, hits in file_report.items():
            report.setdefault(token, []).append((path, hits))
        if count:
            content_changed.append((path, count))
            if not dry_run:
                write(path, new_text.encode("utf-8"))

    # Rename after rewriting content so the reads above all resolve.
    for old, new in renames:
        if dry_run:
            continue
        target_dir = os.path.dirname(os.path.join(REPO_ROOT, new))
        if target_dir and not os.path.isdir(target_dir):
            os.makedirs(target_dir)
        git("mv", old, new)

    total = sum(count for _, count in content_changed)
    verb = "would rewrite" if dry_run else "rewrote"
    print("{} {} tokens across {} files".format(verb, total, len(content_changed)))
    verb = "would rename" if dry_run else "renamed"
    print("{} {} files:".format(verb, len(renames)))
    for old, new in renames:
        print("  {} -> {}".format(old, new))
    print("skipped {} binary files".format(skipped_binary))

    if report:
        print("\nreport-only tokens left in place (resolve by hand, plan section 3.1):")
        for token in sorted(report):
            occurrences = report[token]
            hits = sum(n for _, n in occurrences)
            print("  {}: {} hits in {} files".format(token, hits, len(occurrences)))
    return 0


def run_report():
    """List every ladybugdb / LadybugDB occurrence for hand resolution."""
    found = 0
    for path in tracked_files():
        if not in_scope(path):
            continue
        blob = read(path)
        if is_binary(blob):
            continue
        try:
            text = blob.decode("utf-8")
        except UnicodeDecodeError:
            continue
        for number, line in enumerate(text.splitlines(), start=1):
            for token in REPORT_ONLY:
                if token in line:
                    print("{}:{}: {}".format(path, number, line.strip()))
                    found += 1
                    break
    print("\n{} lines need a hand decision (GitHub org / npm scope / Java group)".format(found))
    return 0


def run_check(profile=()):
    """Exit non-zero if any automatically-renamable token survives."""
    offenders = []
    for path in tracked_files():
        if not in_scope(path):
            continue

        if rename_path(path, profile):
            offenders.append((path, 0, "path still carries a renamable token"))

        blob = read(path)
        if is_binary(blob):
            continue
        try:
            text = blob.decode("utf-8")
        except UnicodeDecodeError:
            continue
        # Re-run the real substitution over the whole file rather than testing for
        # the tokens directly.  Two reasons: `ladybug` is a substring of `ladybugdb`
        # and a report-only occurrence is not an offence, and `skip-next` needs the
        # preceding line to be in scope.  Only a line the rename would actually have
        # changed counts.
        new_text, count, _ = substitute(text, profile)
        if count:
            before = text.splitlines()
            after = new_text.splitlines()
            for number, (old, new) in enumerate(zip(before, after), start=1):
                if old != new:
                    offenders.append((path, number, old.strip()))

    for path, number, detail in offenders:
        print("{}:{}: {}".format(path, number, detail))
    if offenders:
        print("\n{} remaining occurrences".format(len(offenders)))
        return 1
    print("clean: no renamable tokens remain")
    return 0


def run_patch(patch_path, profile=()):
    """Transform a unified diff so it applies to the renamed tree.

    Content lines and the `diff --git` / `---` / `+++` path headers all go
    through the same token table, which is what lets an upstream patch land on
    a tree where both the identifiers and the file names have moved.

    Caveat: an upstream patch carries no `riag-rename:skip` markers, so a
    provenance URL introduced by the patch (`github.com/LadybugDB/ladybug/
    issues/900`) is rewritten into a dead link.  Check the transformed diff for
    URLs before `git am`, and add the marker as part of the pick.
    """
    with open(patch_path, "rb") as handle:
        blob = handle.read()
    text = blob.decode("utf-8", errors="surrogateescape")

    out = []
    for line in text.splitlines(True):
        # Header path lines carry file names; the override table applies to
        # them as well as to the tree.
        if line.startswith(("diff --git ", "--- ", "+++ ", "rename from ", "rename to ")):
            for old, new in RENAME_OVERRIDES.items():
                line = line.replace(old, new)
        new_line, _, _ = substitute(line, profile)
        out.append(new_line)

    # Write bytes: text-mode stdout turns every LF into CRLF on Windows (no patch
    # then applies) and encodes as cp1252 (any non-ASCII hunk then crashes).
    sys.stdout.buffer.write("".join(out).encode("utf-8", "surrogateescape"))
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--dry-run", action="store_true", help="report changes without writing")
    group.add_argument("--check", action="store_true", help="exit 1 if a renamable token remains")
    group.add_argument("--report", action="store_true", help="list ladybugdb occurrences")
    group.add_argument("--patch", metavar="FILE", help="transform a unified diff on stdout")
    parser.add_argument("--root", metavar="DIR",
        help="repository to rename (default: this script's repository). Use it to rename a "
             "binding submodule, which is its own repository.")
    parser.add_argument("--profile", default="core", choices=sorted(PROFILES),
        help="naming profile for the repository being renamed (default: core). See PROFILES.")
    args = parser.parse_args()

    global REPO_ROOT
    if args.root:
        REPO_ROOT = os.path.abspath(args.root)
    profile = PROFILES[args.profile]

    if args.patch:
        return run_patch(args.patch, profile)
    if args.check:
        return run_check(profile)
    if args.report:
        return run_report()
    return run_rename(args.dry_run, profile)


if __name__ == "__main__":
    sys.exit(main())

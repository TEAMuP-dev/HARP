# Reviewing Guidelines

Every pull request on develop requires at least two approvals from other contributors.
The purpose is to catch the routine problems (unclear intent, dead code, broken
behavior, regressions) before the final review, so that review can focus on what remains.

An approval means you read the changes, built the branch, tested it, and found no problems.
If you did not do all of that, do not approve. Approving code makes you partly responsible
for it.

## Table of Contents
* **[Reviewing a Pull Request](#reviewing-a-pull-request)**
    * **[1. Check the Intent](#1-check-the-intent)**
    * **[2. Read the Changes](#2-read-the-changes)**
    * **[3. Test the Branch](#3-test-the-branch)**
    * **[4. Assess Understanding](#4-assess-understanding)**
* **[Leaving the Review](#leaving-the-review)**
* **[Opening a Pull Request](#opening-a-pull-request)**
* **[Reviewer Checklist](#reviewer-checklist)**

---

## Reviewing a Pull Request

### 1. Check the Intent

Read the title and description first.

- Can you state what the changes do in one or two sentences?
- Are the relevant issues linked or referenced?

If not, ask the author to clarify before reviewing further. A pull request that cannot be
explained cannot be reviewed.

### 2. Read the Changes

Skim every changed file, including the ones that look routine.

- **Does the change make sense and read clearly?** Follow the flow. You do not need to
  understand every line to notice that something is happening in the wrong place. Clear
  naming and structure are usually enough on their own, and comments are needed where the
  reasoning is not obvious. "I had to reverse-engineer this" is valid feedback.
- **Is there unnecessary complexity?** Nested conditionals that could be early returns,
  a new class that duplicates an existing one, hand-written code for something the standard
  library or JUCE already provides.
- **Is there unused or leftover code?** Commented-out blocks, debug prints, unused variables,
  functions nothing calls, unresolved TODOs.
- **Is it consistent with the rest of the codebase?** New code should resemble the code
  around it in naming, structure, error handling, and layout. Code should also be formatted
  with `clang-format` against the project `.clang-format` file.
- **Are there accompanying unit tests?** Tests are not required for approval, but they are
  strongly recommended.

You are not expected to catch everything. You are expected to look at everything.

### 3. Test the Branch

This is the most important part of the review

Check out the branch and build it:

```bash
git fetch origin
git checkout <branch-name>
git submodule update --init --recursive

cmake -B build -DCMAKE_BUILD_TYPE=Debug .
cmake --build build --parallel $(nproc)

./build/HARP_artefacts/Debug/HARP
```

Then test it properly.

- **Verify the change works.** Reproduce the original bug and confirm it is fixed, or
  exercise the new feature the way a user would.
- **Try to break it, creatively.** Click things in the wrong order. Cancel partway through.
  Load an empty file, a very large file, a file of the wrong type. Resize the window to
  extremes. Interrupt the network during a request. Use input the author probably did not
  consider.
- **Check functionality that seems unrelated.** Load a model, process audio, process MIDI,
  undo, save, use the clipboard. Regressions rarely appear where the diff is, which is why
  reading alone does not find them.
- **Watch the log.** Errors and warnings in the console or in the app log file
  can sometimes expose problems the interface hides.
- **Test on more than one operating system if you can.** Problems regularly show up on
  macOS or Windows that never appear on Linux, and the other way around. If you only have
  one platform available, state which one you tested on in your review so the gap is visible
  to the next reviewer.

Surface-level testing, meaning the app launched and the new button worked, is the most common
reason a pull request is approved and then turns out to need more work.

When something breaks, report exactly what you did, what you expected, and what happened, so
the author can reproduce it without guessing.

### 4. Assess Understanding

- **Does the author appear to understand the code they wrote?** Inconsistent approaches,
  confused structure, or changes that fix a symptom without an explanation are worth raising.
- **Do you understand it?** If you do not, either the code needs to be clearer or you need to
  ask a question. Do not approve code you do not understand.

---

## Leaving the Review

- **Comment** for questions and optional suggestions.
- **Request changes** for anything that has to be fixed before merging, such as bugs,
  regressions, or code you cannot follow. This is a normal outcome, not a criticism of the
  author.
- **Approve** only when you would be comfortable merging the branch yourself.

Copy the [Reviewer Checklist](#reviewer-checklist) into your review and work through it
there.

Practical points:

- Be specific. Reference the line, give the steps to reproduce, and suggest an alternative
  where you have one.
- Separate blocking issues from optional ones. Differentiate optional remarks.
- Re-check the branch after the author pushes fixes, including a retest if the fix touches
  behavior you tested before.

---

## Opening a Pull Request

- **Write a clear title and description.** State what changed, why, and anything a reviewer
  needs in order to test it, such as setup steps or which model to use.
- **Link the corresponding issues.** Use `Closes #123` in the description, or link the issue
  from the Development section of the pull request page. Mention
  anyone involved in the discussion.
- **Address one issue per pull request.** Small, focused changes get reviewed quickly and
  thoroughly. Unrelated cleanups belong in a separate pull request.
- **Run the reviewer checks on your own code first.** Read your own diff on GitHub from top to
  bottom, then build and test the branch as though someone else wrote it.
    - This step alone catches most problems.
    - Repeat until you find no further problems or improvements.
    - Do not ask anyone to review code you have not reviewed comprehensively yourself.
- **Format your code and confirm CI passes** before requesting review.
- **Open it as a draft** if you want early feedback on unfinished work, and say what kind of
  feedback you are looking for.

---

## Reviewer Checklist

```markdown
- [ ] Title and description make the intent clear
- [ ] Relevant issues are linked
- [ ] The pull request addresses one issue
- [ ] Skimmed every changed file, and the approach makes sense
- [ ] No unnecessary complexity, unused code, or leftover debug output
- [ ] Code is documented or otherwise self-explanatory
- [ ] Consistent with existing code and formatted
- [ ] Tests included where applicable (optional, recommended)
- [ ] Built and ran the branch locally
- [ ] The feature or fix works as described
- [ ] Tried to break it with unexpected input and interactions
- [ ] Checked unrelated functionality for regressions
- [ ] No new errors or warnings in the log
- [ ] Tested on: <operating system>
- [ ] I understand the change well enough to be partly responsible for it
```

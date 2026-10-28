// Fails the build when a page includes a file, or a region of one, that does not exist.
//
// Pages include sections of the repository's README.md and docs/ (see docs/WEBSITE.md). VitePress
// raises no error for a broken include: it leaves the comment of a missing file in place, and
// includes the whole file in place of a missing region, which would put the entire README on a page.

import { existsSync, readFileSync, readdirSync } from 'node:fs'
import { dirname, join, relative, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

// The VitePress source root, against which an include starting with "@" is resolved
const root = resolve(dirname(fileURLToPath(import.meta.url)), '..')

const includeRE = /<!--\s*@include:\s*(.*?)\s*-->/g

// "path", "path#region", or either followed by a line range such as "{3,10}"
const targetRE = /^(.*?)(?:#([\w-]+))?(?:\{\d*,\d*\})?$/

function* markdownFiles(dir) {
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const path = join(dir, entry.name)

    if (entry.isDirectory()) {
      yield* markdownFiles(path)
    } else if (entry.name.endsWith('.md')) {
      yield path
    }
  }
}

// VitePress matches region markers against whole, trimmed lines
const hasLine = (text, line) => text.split(/\r?\n/).some((candidate) => candidate.trim() === line)

const problems = []

for (const page of [join(root, 'index.md'), ...markdownFiles(join(root, 'content'))]) {
  for (const [, target] of readFileSync(page, 'utf-8').matchAll(includeRE)) {
    const [, path, region] = target.match(targetRE)
    const file = path.startsWith('@')
      ? join(root, path.replace(/^@\/?/, ''))
      : resolve(dirname(page), path)
    const where = `${relative(root, page)} includes "${target}"`

    if (!existsSync(file)) {
      problems.push(`${where}, but ${relative(root, file)} does not exist.`)
    } else if (region !== undefined) {
      const text = readFileSync(file, 'utf-8')

      if (!hasLine(text, `<!-- #region ${region} -->`)
          || !hasLine(text, `<!-- #endregion ${region} -->`)) {
        problems.push(`${where}, but ${relative(root, file)} does not mark that region.`)
      }
    }
  }
}

if (problems.length > 0) {
  console.error(problems.join('\n'))
  process.exit(1)
}

# manakan

## Installation

```bash
mkdir -p build && cd build
cmake ..
make
```

```bash
cp config/ $HOME/.config/manakan/
```

### Config

```bash
manakan config --check
```

Layout:

- `config.toml`: `default_provider` only (target defaults live per-provider, see below)
- `providers/*.toml`: one file = one provider. A provider is a self-contained, sendable
  request template with a `[request]` section (`method`/`base_url`/`path`), an optional
  `[headers]` section, and an optional `[data]` section (the HTTP body). Any other
  top-level table is a section a Target may also write into.
- `targets/*.toml`: one file may define several targets sharing the same `use = "<provider>"`.
  A target only supplies values into the sections its provider declared -- it cannot
  invent new top-level sections.

If the configuration directories do not exist, `manakan` asks whether it should create them.
Files under `config/` in this repository are never installed or resolved automatically --
copy the ones you want into your config directory yourself.

- `config.toml`, `providers/httpbingo.toml`, `targets/httpbingo.toml` need no secrets and work
  as committed. `httpbingo` is a plain HTTP API (not a chat service), included to show that a
  provider is just an HTTP request template -- manakan's main use is sending chat messages,
  but it isn't limited to that.
- `providers/discord.toml.example` and `providers/chatwork.toml.example` (with their matching
  `targets/*.toml.example`) need a real webhook URL / token filled in, hence the `.example`
  suffix -- rename them (drop `.example`) once you've supplied your own values.

Provider identity is its `name`; duplicate `name`s across files is an error. Target identity
is `(use, target-name)`; duplicates are an error. A target file may declare `default = "name"`
to mark one of its targets as the default for that `use` -- at most one `default` per provider.

`const` (or its short form `c`) marks a subtree of a Provider as fixed: a Target cannot
override a path under `const`, including by replacing an ancestor table wholesale.

```toml
[request.const]
method = "POST"   # a Target writing request.method is now a config error
```

Placeholder forms:

- `{{PLACEHOLDER}}` -- resolves to any scalar value defined anywhere in the merged
  provider+target tree, by its own key name (regardless of which section it lives in)
- `{{env.NAME}}` -- environment variable
- `{{arg.name}}` -- CLI `--input name=value`
- `{{argv.N}}` -- Nth positional CLI argument (1-indexed)

A value that only exists to be referenced by another placeholder (e.g. a `webhook_path`
used inside `request.path`) should live under `[request]`, not `[data]` -- only
`request.method`/`base_url`/`path` are read out of `[request]`, so anything else placed
there is never sent; anything placed under `[data]` becomes part of the HTTP body.

URL rules:

- `base_url` should contain only scheme + host, such as `https://discord.com`
- if `base_url` ends with `/`, it is normalized internally
- if `path` does not start with `/`, it is normalized internally
- `base_url` containing a non-root path is treated as invalid

Resolution order:

1. Provider: `-p`/`--provider` > `default_provider` in `config.toml` > error
2. Target (within that provider): `-t`/`--target` > the provider's `default` target > the
   sole target if there is exactly one > error

Within CLI input, `--input key=value` overrides a same-named field directly (bypassing
whatever template it held), and takes precedence over positional `argv` references to the
same name.
Example: if TOML uses `content = "{{argv.1}}"`, then `manakan send --input content=xxx yyy`
resolves `content` to `xxx`.

A malformed or unrelated provider/target file does not abort a run; it is reported as a
warning and the rest of the config loads normally. Run `manakan config --check` to see every
warning and validate every provider/target (including `const` violations) without sending
anything.

## Usage

```bash
manakan send --target alerts "message"
manakan s --provider discord --target alerts "message"
manakan send --target dev --input username=test-user "message"
manakan s -i username=test-user -i avatar_url=https://example.com/a.png --target dev "message"

echo "message" | manakan send --target alerts

# Inspect without sending
manakan config --target alerts "message"          # human-readable summary
manakan config --json --target alerts "message"   # resolved request as JSON
manakan config --body --target alerts "message"    # just the serialized body
manakan config --curl --target alerts "message"    # an equivalent curl command
manakan config --check                              # validate the whole config tree
```

## Uninstall

```bash
rm $HOME/.local/bin/manakan
rm $BASH_COMPLETION_USER_DIR/manakan.bash
rm -rf $HOME/.config/manakan
```

## License

manakan is licensed under the MIT License. See `LICENSE`.

## Third-Party Notices

Third-party dependency notices are listed in `THIRD_PARTY_NOTICES.md`.

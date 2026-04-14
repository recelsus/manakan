# manakan

## Installation

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
```

### Binary

```bash
# Install to $HOME/.local/bin by default
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cmake --install build

# Or install system-wide (requires privilege)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMANAKAN_INSTALL_TO_HOME_LOCAL=OFF -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build -j
sudo cmake --install build

# If you already configured build/ before this change, run configure again
cmake -S . -B build -DMANAKAN_INSTALL_TO_HOME_LOCAL=ON

# If you need completion
cp completions/manakan.bash $BASH_COMPLETION_USER_DIR/manakan.bash
```

### Config

```bash
manakan
```

Layout:

- `config.toml`: default provider / target
- `providers/*.toml`: request templates
- `targets/*.toml`: target-specific values

If the configuration directories do not exist, `manakan` asks whether it should create them.
Example files stay in this repository under `config/`; they are not installed automatically.

Placeholder forms:

- `{{PLACEHOLDER}}`
- `{{env.NAME}}`
- `{{arg.name}}`
- `{{argv.N}}`

URL rules:

- `base_url` should contain only scheme + host, such as `https://discord.com`
- if `base_url` ends with `/`, it is normalized internally
- if `path` does not start with `/`, it is normalized internally
- `base_url` containing a non-root path is treated as invalid

Resolution order:

- CLI
- target
- provider

Within CLI input, `--input key=value` takes precedence over positional `argv`.
Example: if TOML uses `content = "{{argv.1}}"`, then `manakan --input content=xxx yyy`
resolves `content` to `xxx`.

## Usage

```bash
manakan --target alerts "message"
manakan --provider discord --target alerts "message"
manakan --target dev --input username=test-user "message"
manakan -i username=test-user -i avatar_url=https://example.com/a.png --target dev "message"

echo "message" | manakan --target alerts
```

## Uninstall

```bash
rm $HOME/.local/bin/manakan
rm $BASH_COMPLETION_USER_DIR/manakan.bash
rm -rf $HOME/.config/manakan
```

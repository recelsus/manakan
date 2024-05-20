# manakan

## Installation

```bash
make
```
### Binary

```bash
# Copy to directory included in PATH
cp bin/manakan $HOME/.local/bin/manakan

# If you need completion.
cp completions/manakan_completions.sh $HOME/.local/share/bash-completion/manakan_completions.sh 
```
### Config

Write the header and data using TOML notation.  
For messages you want to send dynamically, write the key name.  
[Config Example](https://github.com/recelsus/manakan/blob/master/config/manakan.toml.example)
```bash
cp config/manakan.toml.example $XDG_CONFIG_HOME/manakan/manakan.toml 
or
cp config/manakan.toml.example $HOME/.config/manakan/manakan.toml 
```

## Usage

```bash
# The first host written in TOML will be selected.
manakan "message"
# You can specify the host by adding the option.
manakan -t example-discord "message"

echo "message" | manakan 
echo "message" | manakan -t example-discord
```

## Uninstall
```bash
rm $HOME/.local/bin/manakan
rm $BASH_COMPLETION_USER_DIR/manakan_completions.bash
rm -rf $HOME/.config/manakan
```
